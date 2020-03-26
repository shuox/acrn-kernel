// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (SRV): ioeventfd based on eventfd
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Shuo A Liu <shuo.a.liu@intel.com>
 * 		Yakui Zhao <yakui.zhao@intel.com>
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
#include "acrn_drv.h"

/* use internally to record properties of each ioeventfd */
struct hsm_ioeventfd {
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

/* assumes ioeventfds_lock held */
static void acrn_ioeventfd_shutdown(struct hsm_ioeventfd *p)
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

/* assumes ioeventfds_lock held */
static bool hsm_ioeventfd_is_duplicated(struct acrn_vm *vm,
					 struct hsm_ioeventfd *ioeventfd)
{
	struct hsm_ioeventfd *p;

	/*
	 * Treat same addr/type/data with different length combination
	 * as the same one.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   PIO[0x100~0x103] with data 0x10 will be failed to register.
	 */
	list_for_each_entry(p, &vm->ioeventfds, list)
		if (p->addr == ioeventfd->addr &&
		    p->type == ioeventfd->type &&
		    (p->wildcard || ioeventfd->wildcard ||
		     p->data == ioeventfd->data))
			return true;

	return false;
}

static int acrn_assign_ioeventfd(struct acrn_vm *vm,
				struct acrn_ioeventfd *args)
{
	struct eventfd_ctx *eventfd;
	struct hsm_ioeventfd *p;
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

	mutex_lock(&vm->ioeventfds_lock);

	/* Verify that there isn't a match already */
	if (hsm_ioeventfd_is_duplicated(vm, p)) {
		ret = -EEXIST;
		goto unlock_fail;
	}

	/* register the IO range into acrn client */
	ret = acrn_ioreq_add_range(vm->ioeventfd_client, p->type,
				     p->addr, p->addr + p->length - 1);
	if (ret < 0)
		goto unlock_fail;

	list_add_tail(&p->list, &vm->ioeventfds);
	mutex_unlock(&vm->ioeventfds_lock);

	return 0;

unlock_fail:
	mutex_unlock(&vm->ioeventfds_lock);
	kfree(p);
fail:
	eventfd_ctx_put(eventfd);
	return ret;
}

static int acrn_deassign_ioeventfd(struct acrn_vm *vm,
				   struct acrn_ioeventfd *args)
{
	struct hsm_ioeventfd *p;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&vm->ioeventfds_lock);
	list_for_each_entry(p, &vm->ioeventfds, list) {
		if (p->eventfd != eventfd)
			continue;

		acrn_ioreq_del_range(vm->ioeventfd_client, p->type,
					p->addr, p->addr + p->length - 1);
		acrn_ioeventfd_shutdown(p);
		break;
	}
	mutex_unlock(&vm->ioeventfds_lock);

	eventfd_ctx_put(eventfd);
	return 0;
}

static struct hsm_ioeventfd *
hsm_ioeventfd_match(struct acrn_vm *vm, u64 addr, u64 data, int len, int type)
{
	struct hsm_ioeventfd *p = NULL;

	/*
	 * Same addr/type/data will be treated as hit, otherwise ignore.
	 *   Register PIO[0x100~0x107] with data 0x10 as ioeventfd A, later
	 *   request PIO[0x100~0x103] with data 0x10 will hit A.
	 */
	list_for_each_entry(p, &vm->ioeventfds, list) {
		if (p->type == type && p->addr == addr &&
		    (p->wildcard || p->data == data))
			return p;
	}

	return NULL;
}

static int acrn_ioeventfd_handler(struct ioreq_client *client,
				struct acrn_request *req)
{
	struct hsm_ioeventfd *p;
	u64 addr;
	u64 val;
	int size;

	if (req->type == REQ_MMIO) {
		if (req->reqs.mmio_request.direction == REQUEST_READ) {
			/* reading does nothing and return 0 */
			req->reqs.mmio_request.value = 0;
			return 0;
		}
		addr = req->reqs.mmio_request.address;
		size = req->reqs.mmio_request.size;
		val = req->reqs.mmio_request.value;
	} else {
		if (req->reqs.pio_request.direction == REQUEST_READ) {
			/* reading does nothing and return 0 */
			req->reqs.pio_request.value = 0;
			return 0;
		}
		addr = req->reqs.pio_request.address;
		size = req->reqs.pio_request.size;
		val = req->reqs.pio_request.value;
	}

	mutex_lock(&client->vm->ioeventfds_lock);
	p = hsm_ioeventfd_match(client->vm, addr, val, size, req->type);
	if (p)
		eventfd_signal(p->eventfd, 1);
	mutex_unlock(&client->vm->ioeventfds_lock);

	return 0;
}

int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args)
{
	int ret;

	if (args->flags & ACRN_IOEVENTFD_FLAG_DEASSIGN)
		ret = acrn_deassign_ioeventfd(vm, args);
	else
		ret = acrn_assign_ioeventfd(vm, args);

	return ret;
}

int acrn_ioeventfd_init(struct acrn_vm *vm)
{
	char name[16];

	mutex_init(&vm->ioeventfds_lock);
	INIT_LIST_HEAD(&vm->ioeventfds);
	snprintf(name, sizeof(name), "ioeventfd-%hu", vm->vmid);
	vm->ioeventfd_client = acrn_ioreq_create_client(vm,
				acrn_ioeventfd_handler, NULL, false, name);
	if (!vm->ioeventfd_client) {
		pr_err("acrn: Failed to create ioeventfd ioreq client!\n");
		return -EINVAL;
	}

	pr_info("acrn: ioeventfd init done!\n");
	return 0;
}

void acrn_ioeventfd_deinit(struct acrn_vm *vm)
{
	struct hsm_ioeventfd *p, *next;

	acrn_ioreq_destroy_client(vm->ioeventfd_client);
	mutex_lock(&vm->ioeventfds_lock);
	list_for_each_entry_safe(p, next, &vm->ioeventfds, list)
		acrn_ioeventfd_shutdown(p);
	mutex_unlock(&vm->ioeventfds_lock);
}
