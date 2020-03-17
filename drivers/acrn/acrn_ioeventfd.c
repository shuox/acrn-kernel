// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (SRV): ioeventfd based on eventfd
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 * Liu Shuo <shuo.a.liu@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 */
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/eventfd.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/acrn.h>
#include <linux/acrn_host.h>

#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

static LIST_HEAD(acrn_ioeventfd_clients);
static DEFINE_MUTEX(acrn_ioeventfds_mutex);

/* use internally to record properties of each ioeventfd */
struct acrn_hsm_ioeventfd {
	/* list to link all ioventfd together */
	struct list_head list;
	/* eventfd of this ioeventfd */
	struct eventfd_ctx *eventfd;
	/* start address for IO range*/
	u64 addr;
	/* match data */
	u64 data;
	/* length for IO range */
	int length;
	/* IO range type, can be REQ_PORTIO and REQ_MMIO */
	int type;
	/* ignore match data if true */
	bool wildcard;
};

/* instance to bind ioeventfds of each VM */
struct acrn_ioeventfd_info {
	struct list_head list;
	atomic_t refcnt;
	/* vmid of VM */
	unsigned short vmid;
	/* acrn ioreq client for this instance */
	int acrn_client_id;
	/* vcpu number of this VM */
	int vcpu_num;
	/* ioreq shared buffer of this VM */
	struct acrn_request *req_buf;

	/* the mutex lock to protect ioeventfd list attached to VM */
	struct mutex ioeventfds_lock;
	/* ioeventfds in this instance */
	struct list_head ioeventfds;
};

static
struct acrn_ioeventfd_info *get_ioeventfd_info_by_vm(unsigned short vmid)
{
	struct acrn_ioeventfd_info *info = NULL;

	mutex_lock(&acrn_ioeventfds_mutex);
	list_for_each_entry(info, &acrn_ioeventfd_clients, list) {
		if (info->vmid == vmid) {
			atomic_inc(&info->refcnt);
			mutex_unlock(&acrn_ioeventfds_mutex);
			return info;
		}
	}
	mutex_unlock(&acrn_ioeventfds_mutex);
	return NULL;
}

static void put_ioeventfd_info(struct acrn_ioeventfd_info *info)
{
	mutex_lock(&acrn_ioeventfds_mutex);
	if (atomic_dec_and_test(&info->refcnt)) {
		list_del(&info->list);
		mutex_unlock(&acrn_ioeventfds_mutex);
		kfree(info);
		return;
	}
	mutex_unlock(&acrn_ioeventfds_mutex);
}

/* assumes info->ioeventfds_lock held */
static void acrn_ioeventfd_shutdown(struct acrn_hsm_ioeventfd *p)
{
	eventfd_ctx_put(p->eventfd);
	list_del(&p->list);
	kfree(p);
}

static inline int ioreq_type_from_flags(int flags)
{
	return flags & ACRN_IOEVENTFD_FLAG_PIO ?
			REQ_PORTIO : REQ_MMIO;
}

/* assumes info->ioeventfds_lock held */
static bool acrn_ioeventfd_is_duplicated(struct acrn_ioeventfd_info *info,
					 struct acrn_hsm_ioeventfd *ioeventfd)
{
	struct acrn_hsm_ioeventfd *p;

	/*
	 * Treat same addr/type/data with different length combination
	 * as the same one.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   PIO[0x100~0x103] with data 0x10 will be failed to register.
	 */
	list_for_each_entry(p, &info->ioeventfds, list)
		if (p->addr == ioeventfd->addr &&
		    p->type == ioeventfd->type &&
		    (p->wildcard || ioeventfd->wildcard ||
		     p->data == ioeventfd->data))
			return true;

	return false;
}

static int acrn_assign_ioeventfd(struct acrn_ioeventfd_info *info,
				 struct acrn_ioeventfd *args)
{
	struct eventfd_ctx *eventfd;
	struct acrn_hsm_ioeventfd *p;
	int ret = -ENOENT;

	/* check for range overflow */
	if (args->addr + args->len < args->addr)
		return -EINVAL;

	/* Only support 1,2,4,8 width registers */
	if (!(args->len == 1 || args->len == 2 ||
	      args->len == 4 || args->len == 8))
		return -EINVAL;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&p->list);
	p->addr    = args->addr;
	p->length  = args->len;
	p->eventfd = eventfd;
	p->type	   = ioreq_type_from_flags(args->flags);

	/* If datamatch enabled, we compare the data
	 * otherwise this is a wildcard
	 */
	if (args->flags & ACRN_IOEVENTFD_FLAG_DATAMATCH)
		p->data = args->data;
	else
		p->wildcard = true;

	mutex_lock(&info->ioeventfds_lock);

	/* Verify that there isn't a match already */
	if (acrn_ioeventfd_is_duplicated(info, p)) {
		ret = -EEXIST;
		goto unlock_fail;
	}

	/* register the IO range into acrn client */
	ret = acrn_ioreq_add_iorange(info->acrn_client_id, p->type,
				     p->addr, p->addr + p->length - 1);
	if (ret < 0)
		goto unlock_fail;

	list_add_tail(&p->list, &info->ioeventfds);
	mutex_unlock(&info->ioeventfds_lock);

	return 0;

unlock_fail:
	mutex_unlock(&info->ioeventfds_lock);
fail:
	kfree(p);
	eventfd_ctx_put(eventfd);
	return ret;
}

static int acrn_deassign_ioeventfd(struct acrn_ioeventfd_info *info,
				   struct acrn_ioeventfd *args)
{
	struct acrn_hsm_ioeventfd *p, *tmp;
	struct eventfd_ctx *eventfd;
	int ret = 0;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&info->ioeventfds_lock);

	list_for_each_entry_safe(p, tmp, &info->ioeventfds, list) {
		if (p->eventfd != eventfd)
			continue;

		ret = acrn_ioreq_del_iorange(info->acrn_client_id, p->type,
					     p->addr,
					     p->addr + p->length - 1);
		if (ret)
			break;
		acrn_ioeventfd_shutdown(p);
		break;
	}

	mutex_unlock(&info->ioeventfds_lock);

	eventfd_ctx_put(eventfd);

	return ret;
}

static struct acrn_hsm_ioeventfd *
acrn_ioeventfd_match(struct acrn_ioeventfd_info *info,
		     u64 addr, u64 data,
		     int length, int type)
{
	struct acrn_hsm_ioeventfd *p = NULL;

	/*
	 * Same addr/type/data will be treated as hit, otherwise ignore.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   request PIO[0x100~0x103] with data 0x10 will hit A.
	 */
	list_for_each_entry(p, &info->ioeventfds, list) {
		if (p->type == type && p->addr == addr &&
		    (p->wildcard || p->data == data))
			return p;
	}

	return NULL;
}

static int acrn_ioeventfd_handler(int client_id,
				  unsigned long *ioreqs_map,
				  void *client_priv)
{
	struct acrn_request *req;
	struct acrn_hsm_ioeventfd *p;
	struct acrn_ioeventfd_info *info;
	u64 addr;
	u64 val;
	int size;
	int vcpu;

	info = (struct acrn_ioeventfd_info *)client_priv;
	if (!info)
		return -EINVAL;

	/* get req buf */
	if (!info->req_buf) {
		info->req_buf = acrn_ioreq_get_reqbuf(info->acrn_client_id);
		if (!info->req_buf) {
			pr_err("Failed to get req_buf for client %d\n",
			       info->acrn_client_id);
			return -EINVAL;
		}
	}

	while (1) {
		vcpu = find_first_bit(ioreqs_map, info->vcpu_num);
		if (vcpu == info->vcpu_num)
			break;
		req = &info->req_buf[vcpu];
		if (atomic_read(&req->processed) == REQ_STATE_PROCESSING &&
		    req->client == client_id) {
			if (req->type == REQ_MMIO) {
				if (req->reqs.mmio_request.direction ==
						REQUEST_READ) {
					/* reading does nothing and return 0 */
					req->reqs.mmio_request.value = 0;
					goto next_ioreq;
				}
				addr = req->reqs.mmio_request.address;
				size = req->reqs.mmio_request.size;
				val = req->reqs.mmio_request.value;
			} else {
				if (req->reqs.pio_request.direction ==
						REQUEST_READ) {
					/* reading does nothing and return 0 */
					req->reqs.pio_request.value = 0;
					goto next_ioreq;
				}
				addr = req->reqs.pio_request.address;
				size = req->reqs.pio_request.size;
				val = req->reqs.pio_request.value;
			}

			mutex_lock(&info->ioeventfds_lock);
			p = acrn_ioeventfd_match(info, addr, val, size,
						 req->type);
			if (p)
				eventfd_signal(p->eventfd, 1);
			mutex_unlock(&info->ioeventfds_lock);

next_ioreq:
			acrn_ioreq_complete_request(client_id, vcpu, req);
		}
	}

	return 0;
}

int acrn_ioeventfd_init(unsigned short vmid)
{
	int ret = 0;
	char name[16];
	struct acrn_ioeventfd_info *info;

	info = get_ioeventfd_info_by_vm(vmid);
	if (info) {
		put_ioeventfd_info(info);
		return -EEXIST;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	mutex_init(&info->ioeventfds_lock);
	info->vmid = vmid;
	atomic_set(&info->refcnt, 1);
	INIT_LIST_HEAD(&info->ioeventfds);
	info->vcpu_num = ACRN_REQUEST_MAX;

	snprintf(name, sizeof(name), "ioeventfd-%hu", vmid);
	info->acrn_client_id = acrn_ioreq_create_client(vmid,
							acrn_ioeventfd_handler,
							info, name);
	if (info->acrn_client_id < 0) {
		pr_err("Failed to create ioeventfd client for ioreq!\n");
		ret = -EINVAL;
		goto fail;
	}

	ret = acrn_ioreq_attach_client(info->acrn_client_id);
	if (ret < 0) {
		pr_err("Failed to attach acrn client %d!\n",
		       info->acrn_client_id);
		goto client_fail;
	}

	mutex_lock(&acrn_ioeventfds_mutex);
	list_add(&info->list, &acrn_ioeventfd_clients);
	mutex_unlock(&acrn_ioeventfds_mutex);

	pr_info("ACRN hsm ioeventfd init done!\n");
	return 0;
client_fail:
	acrn_ioreq_destroy_client(info->acrn_client_id);
fail:
	kfree(info);
	return ret;
}

void acrn_ioeventfd_deinit(unsigned short vmid)
{
	struct acrn_hsm_ioeventfd *p, *tmp;
	struct acrn_ioeventfd_info *info = NULL;

	info = get_ioeventfd_info_by_vm(vmid);
	if (!info)
		return;

	acrn_ioreq_destroy_client(info->acrn_client_id);
	mutex_lock(&info->ioeventfds_lock);
	list_for_each_entry_safe(p, tmp, &info->ioeventfds, list)
		acrn_ioeventfd_shutdown(p);
	mutex_unlock(&info->ioeventfds_lock);

	put_ioeventfd_info(info);
	/* put one more as we count it in finding */
	put_ioeventfd_info(info);
}

int acrn_ioeventfd_config(unsigned short vmid, struct acrn_ioeventfd *args)
{
	struct acrn_ioeventfd_info *info = NULL;
	int ret;

	info = get_ioeventfd_info_by_vm(vmid);
	if (!info)
		return -ENOENT;

	if (args->flags & ACRN_IOEVENTFD_FLAG_DEASSIGN)
		ret = acrn_deassign_ioeventfd(info, args);
	else
		ret = acrn_assign_ioeventfd(info, args);

	put_ioeventfd_info(info);
	return ret;
}
