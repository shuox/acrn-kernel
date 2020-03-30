// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN HSM: ioeventfd
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Shuo A Liu <shuo.a.liu@intel.com>
 *	Yakui Zhao <yakui.zhao@intel.com>
 */
#include <linux/eventfd.h>
#include <linux/slab.h>

#include "acrn_drv.h"

struct hsm_ioeventfd {
	struct list_head list;
	/* eventfd of ioeventfd object */
	struct eventfd_ctx *eventfd;
	/* Address of I/O range */
	u64 addr;
	/* Data for matching */
	u64 data;
	/* Length of I/O range */
	int length;
	/* Type of I/O range */
	int type;
	/* Data matching or not */
	bool wildcard;
};

static inline int ioreq_type_from_flags(int flags)
{
	return flags & ACRN_IOEVENTFD_FLAG_PIO ?
			ACRN_IOREQ_TYPE_PORTIO : ACRN_IOREQ_TYPE_MMIO;
}

/* assumes ioeventfds_lock held */
static void acrn_ioeventfd_shutdown(struct hsm_ioeventfd *p)
{
	eventfd_ctx_put(p->eventfd);
	list_del(&p->list);
	kfree(p);
}

/* assumes ioeventfds_lock held */
static bool hsm_ioeventfd_is_duplicated(struct acrn_vm *vm,
					 struct hsm_ioeventfd *ioeventfd)
{
	struct hsm_ioeventfd *p;

	list_for_each_entry(p, &vm->ioeventfds, list)
		if (p->eventfd == ioeventfd->eventfd &&
		    p->addr == ioeventfd->addr &&
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
	int ret;

	/* check for range overflow */
	if (args->addr + args->len < args->addr)
		return -EINVAL;

	/*
	 * Currently, acrn_ioeventfd is used to support vhost. 1,2,4,8 width
	 * accesses can cover vhost's requirements.
	 */
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

	/*
	 * ACRN_IOEVENTFD_FLAG_DATAMATCH flag is set in virtio 1.0 support, the
	 * writing of notification register of each virtqueue may trigger the
	 * notification. There is no data matching requirement.
	 */
	if (args->flags & ACRN_IOEVENTFD_FLAG_DATAMATCH)
		p->data = args->data;
	else
		p->wildcard = true;

	mutex_lock(&vm->ioeventfds_lock);

	if (hsm_ioeventfd_is_duplicated(vm, p)) {
		ret = -EEXIST;
		goto unlock_fail;
	}

	/* register the I/O range into ioreq client */
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

	list_for_each_entry(p, &vm->ioeventfds, list) {
		if (p->type == type && p->addr == addr && p->length >= len &&
			(p->wildcard || p->data == data))
			return p;
	}

	return NULL;
}

static int acrn_ioeventfd_handler(struct acrn_ioreq_client *client,
				struct acrn_io_request *req)
{
	struct hsm_ioeventfd *p;
	u64 addr;
	u64 val;
	int size;

	if (req->type == ACRN_IOREQ_TYPE_MMIO) {
		/*
		 * I/O requests are dispatched by range check only, so a
		 * acrn_ioreq_client need process both READ and WRITE accesses
		 * of same range. READ accesses are safe to be ignored here
		 * because virtio PCI devices write the notify registers for
		 * notification.
		 */
		if (req->reqs.mmio_request.direction == ACRN_IOREQ_DIR_READ) {
			/* reading does nothing and return 0 */
			req->reqs.mmio_request.value = 0;
			return 0;
		}
		addr = req->reqs.mmio_request.address;
		size = req->reqs.mmio_request.size;
		val = req->reqs.mmio_request.value;
	} else {
		if (req->reqs.pio_request.direction == ACRN_IOREQ_DIR_READ) {
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
	char name[ACRN_NAME_LEN];

	mutex_init(&vm->ioeventfds_lock);
	INIT_LIST_HEAD(&vm->ioeventfds);
	snprintf(name, sizeof(name), "ioeventfd-%hu", vm->vmid);
	vm->ioeventfd_client = acrn_ioreq_create_client(vm,
				acrn_ioeventfd_handler, NULL, false, name);
	if (!vm->ioeventfd_client) {
		pr_err("acrn: Failed to create ioeventfd ioreq client!\n");
		return -EINVAL;
	}

	pr_debug("acrn: VM %d ioeventfd init.\n", vm->vmid);
	return 0;
}

void acrn_ioeventfd_deinit(struct acrn_vm *vm)
{
	struct hsm_ioeventfd *p, *next;

	pr_debug("acrn: VM %d ioeventfd deinit.\n", vm->vmid);
	acrn_ioreq_destroy_client(vm->ioeventfd_client);
	mutex_lock(&vm->ioeventfds_lock);
	list_for_each_entry_safe(p, next, &vm->ioeventfds, list)
		acrn_ioeventfd_shutdown(p);
	mutex_unlock(&vm->ioeventfds_lock);
}
