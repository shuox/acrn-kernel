// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN_HSM: handle the ioreq_request in ioreq_client
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 * Jack Ren <jack.ren@intel.com>
 * FengWei yin <fengwei.yin@intel.com>
 *
 */

#include <asm/io.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/acrn.h>
#include <linux/acrn_host.h>

#include <linux/idr.h>
#include <linux/refcount.h>
#include <linux/rwlock_types.h>

#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

/* rwlock that is used to protect IDR client */
static DEFINE_RWLOCK(client_lock);
static struct idr	idr_client;

struct ioreq_range {
	struct list_head list;
	u32 type;
	long start;
	long end;
};

enum IOREQ_CLIENT_BITS {
	IOREQ_CLIENT_DESTROYING = 0,
	IOREQ_CLIENT_EXIT,
};

struct ioreq_client {
	/* client name */
	char name[16];
	/* client id */
	int id;
	/* vm this client belongs to */
	unsigned short vmid;
	/* list node for this ioreq_client */
	struct list_head list;
	/*
	 * is this client fallback?
	 * there is only one fallback client in a vm - dm
	 * a fallback client shares IOReq buffer pages
	 * a fallback client handles all left IOReq not handled by other clients
	 * a fallback client does not need add io ranges
	 * a fallback client handles ioreq in its own context
	 */
	bool fallback;

	unsigned long flags;

	/* client covered io ranges - N/A for fallback client */
	struct list_head range_list;
	rwlock_t range_lock;

	/*
	 *   this req records the req number this client need handle
	 */
	DECLARE_BITMAP(ioreqs_map, ACRN_REQUEST_MAX);

	/*
	 * client ioreq handler:
	 *   if client provides a handler, it means acrn need create a kthread
	 *   to call the handler while there is ioreq.
	 *   if client doesn't provide a handler, client should handle ioreq
	 *   in its own context when calls acrn_ioreq_attach_client.
	 *
	 *   NOTE: for fallback client, there is no ioreq handler.
	 */
	ioreq_handler_t handler;
	bool acrn_create_kthread;
	struct task_struct *thread;
	wait_queue_head_t wq;

	refcount_t refcnt;
	/* add the ref_vm of ioreq_client */
	struct acrn_vm *ref_vm;
	void *client_priv;
};

#define MAX_CLIENT 1024
static void acrn_ioreq_notify_client(struct ioreq_client *client);

static inline bool has_pending_request(struct ioreq_client *client)
{
	if (client)
		return !bitmap_empty(client->ioreqs_map, ACRN_REQUEST_MAX);
	else
		return false;
}

static int alloc_client(void)
{
	struct ioreq_client *client;
	int ret;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;
	refcount_set(&client->refcnt, 1);

	write_lock_bh(&client_lock);
	ret = idr_alloc_cyclic(&idr_client, client, 1, MAX_CLIENT, GFP_NOWAIT);
	write_unlock_bh(&client_lock);

	if (ret < 0) {
		kfree(client);
		return -EINVAL;
	}

	client->id = ret;

	return ret;
}

static struct ioreq_client *acrn_ioreq_get_client(int client_id)
{
	struct ioreq_client *obj;

	read_lock_bh(&client_lock);
	obj = idr_find(&idr_client, client_id);
	if (obj)
		refcount_inc(&obj->refcnt);
	read_unlock_bh(&client_lock);

	return obj;
}

static void acrn_ioreq_put_client(struct ioreq_client *client)
{
	if (refcount_dec_and_test(&client->refcnt)) {
		struct acrn_vm *ref_vm = client->ref_vm;
		/* The client should be released when refcnt = 0 */
		kfree(client);
		put_vm(ref_vm);
	}
}

int acrn_ioreq_create_client(unsigned short vmid,
			     ioreq_handler_t handler,
			     void *client_priv,
			     char *name)
{
	struct acrn_vm *vm;
	struct ioreq_client *client;
	int client_id;

	might_sleep();

	vm = find_get_vm(vmid);
	if (unlikely(!vm || !vm->req_buf)) {
		pr_err("acrn-ioreq: failed to find vm from vmid %d\n", vmid);
		if (vm)
			put_vm(vm);
		return -EINVAL;
	}

	client_id = alloc_client();
	if (unlikely(client_id < 0)) {
		pr_err("acrn-ioreq: vm[%d] failed to alloc ioreq client\n",
		       vmid);
		put_vm(vm);
		return -EINVAL;
	}

	client = acrn_ioreq_get_client(client_id);
	if (unlikely(!client)) {
		pr_err("failed to get the client.\n");
		put_vm(vm);
		return -EINVAL;
	}

	if (handler) {
		client->handler = handler;
		client->acrn_create_kthread = true;
	}

	client->ref_vm = vm;
	client->vmid = vmid;
	client->client_priv = client_priv;
	if (name)
		strncpy(client->name, name, sizeof(client->name) - 1);
	rwlock_init(&client->range_lock);
	INIT_LIST_HEAD(&client->range_list);
	init_waitqueue_head(&client->wq);

	/* When it is added to ioreq_client_list, the refcnt is increased */
	spin_lock_bh(&vm->ioreq_client_lock);
	list_add(&client->list, &vm->ioreq_client_list);
	spin_unlock_bh(&vm->ioreq_client_lock);

	pr_info("acrn-ioreq: created ioreq client %d\n", client_id);

	return client_id;
}

void acrn_ioreq_clear_request(struct acrn_vm *vm)
{
	struct ioreq_client *client;
	struct list_head *pos;
	bool has_pending = false;
	int retry_cnt = 10;
	int bit;

	/*
	 * Now, ioreq clearing only happens when do VM reset. Current
	 * implementation is waiting all ioreq clients except the DM
	 * one have no pending ioreqs in 10ms per loop
	 */

	do {
		spin_lock_bh(&vm->ioreq_client_lock);
		list_for_each(pos, &vm->ioreq_client_list) {
			client = container_of(pos, struct ioreq_client, list);
			if (vm->ioreq_fallback_client == client->id)
				continue;
			has_pending = has_pending_request(client);
			if (has_pending)
				break;
		}
		spin_unlock_bh(&vm->ioreq_client_lock);

		if (has_pending)
			schedule_timeout_interruptible(HZ / 100);
	} while (has_pending && --retry_cnt > 0);

	if (retry_cnt == 0)
		pr_warn("ioreq client[%d] cannot flush pending request!\n",
			client->id);

	/* Clear all ioreqs belong to DM. */
	if (vm->ioreq_fallback_client > 0) {
		client = acrn_ioreq_get_client(vm->ioreq_fallback_client);
		if (!client)
			return;

		bit = find_next_bit(client->ioreqs_map, ACRN_REQUEST_MAX, 0);
		while (bit < ACRN_REQUEST_MAX) {
			acrn_ioreq_complete_request(client->id, bit, NULL);
			bit = find_next_bit(client->ioreqs_map,
					    ACRN_REQUEST_MAX,
					    bit + 1);
		}
		acrn_ioreq_put_client(client);
	}
}

int acrn_ioreq_create_fallback_client(unsigned short vmid, char *name)
{
	struct acrn_vm *vm;
	int client_id;
	struct ioreq_client *client;

	vm = find_get_vm(vmid);
	if (unlikely(!vm)) {
		pr_err("acrn-ioreq: failed to find vm from vmid %d\n",
		       vmid);
		return -EINVAL;
	}

	if (unlikely(vm->ioreq_fallback_client > 0)) {
		pr_err("acrn-ioreq: there is already fallback client exist for vm %d\n",
		       vmid);
		put_vm(vm);
		return -EINVAL;
	}

	client_id = acrn_ioreq_create_client(vmid, NULL, NULL, name);
	if (unlikely(client_id < 0)) {
		put_vm(vm);
		return -EINVAL;
	}

	client = acrn_ioreq_get_client(client_id);
	if (unlikely(!client)) {
		pr_err("failed to get the client.\n");
		put_vm(vm);
		return -EINVAL;
	}

	client->fallback = true;
	vm->ioreq_fallback_client = client_id;

	acrn_ioreq_put_client(client);
	put_vm(vm);

	return client_id;
}

/* When one client is removed from VM, the refcnt is decreased */
static void acrn_ioreq_remove_client_pervm(struct ioreq_client *client,
					   struct acrn_vm *vm)
{
	struct list_head *pos, *tmp;

	set_bit(IOREQ_CLIENT_DESTROYING, &client->flags);

	if (client->acrn_create_kthread) {
		/* when the kthread is already started, the kthread_stop is
		 * used to terminate the ioreq_client_thread
		 */
		if (client->thread &&
		    !test_bit(IOREQ_CLIENT_EXIT, &client->flags))
			kthread_stop(client->thread);

		/* decrease the refcount as it is increased when creating
		 * ioreq_client_thread kthread
		 */
		acrn_ioreq_put_client(client);
	} else {
		set_bit(IOREQ_CLIENT_DESTROYING, &client->flags);
		acrn_ioreq_notify_client(client);
	}

	write_lock_bh(&client->range_lock);
	list_for_each_safe(pos, tmp, &client->range_list) {
		struct ioreq_range *range =
			container_of(pos, struct ioreq_range, list);
		list_del(&range->list);
		kfree(range);
	}
	write_unlock_bh(&client->range_lock);

	spin_lock_bh(&vm->ioreq_client_lock);
	list_del(&client->list);
	spin_unlock_bh(&vm->ioreq_client_lock);

	if (client->id == vm->ioreq_fallback_client)
		vm->ioreq_fallback_client = -1;

	acrn_ioreq_put_client(client);
}

void acrn_ioreq_destroy_client(int client_id)
{
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return;
	}

	write_lock_bh(&client_lock);
	client = idr_remove(&idr_client, client_id);
	write_unlock_bh(&client_lock);

	/* When client_id is released, just keep silence and return */
	if (!client)
		return;

	might_sleep();

	acrn_ioreq_remove_client_pervm(client, client->ref_vm);
	acrn_ioreq_put_client(client);
}

/*
 * NOTE: here just add iorange entry directly, no check for the overlap..
 * please client take care of it
 */
int acrn_ioreq_add_iorange(int client_id, uint32_t type,
			   long start, long end)
{
	struct ioreq_client *client;
	struct ioreq_range *range;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	if (end < start) {
		pr_err("acrn-ioreq: end < start\n");
		return -EFAULT;
	}

	client = acrn_ioreq_get_client(client_id);
	if (!client) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	might_sleep();

	range = kzalloc(sizeof(*range), GFP_KERNEL);
	if (!range) {
		acrn_ioreq_put_client(client);
		return -ENOMEM;
	}
	range->type = type;
	range->start = start;
	range->end = end;

	write_lock_bh(&client->range_lock);
	list_add(&range->list, &client->range_list);
	write_unlock_bh(&client->range_lock);
	acrn_ioreq_put_client(client);

	return 0;
}

int acrn_ioreq_del_iorange(int client_id, uint32_t type,
			   long start, long end)
{
	struct ioreq_client *client;
	struct ioreq_range *range;
	struct list_head *pos, *tmp;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	if (end < start) {
		pr_err("acrn-ioreq: end < start\n");
		return -EFAULT;
	}

	client = acrn_ioreq_get_client(client_id);
	if (!client) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	might_sleep();

	read_lock_bh(&client->range_lock);
	list_for_each_safe(pos, tmp, &client->range_list) {
		range = container_of(pos, struct ioreq_range, list);
		if ((range->type == type) &&
		    (start == range->start) &&
		    (end == range->end)) {
			list_del(&range->list);
			kfree(range);
			break;
		}
	}
	read_unlock_bh(&client->range_lock);
	acrn_ioreq_put_client(client);

	return 0;
}

static inline bool is_destroying(struct ioreq_client *client)
{
	if (client)
		return test_bit(IOREQ_CLIENT_DESTROYING, &client->flags);
	else
		return true;
}

struct acrn_request *acrn_ioreq_get_reqbuf(int client_id)
{
	struct ioreq_client *client;
	struct acrn_vm *vm;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return NULL;
	}
	client = acrn_ioreq_get_client(client_id);
	if (!client) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return NULL;
	}

	vm = client->ref_vm;
	if (unlikely(!vm || !vm->req_buf)) {
		pr_err("acrn-ioreq: failed to find vm from vmid %d\n",
		       client->vmid);
		acrn_ioreq_put_client(client);
		return NULL;
	}

	acrn_ioreq_put_client(client);
	return (struct acrn_request *)vm->req_buf;
}

static int ioreq_client_thread(void *data)
{
	struct ioreq_client *client;
	int ret;

	client = (struct ioreq_client *)data;

	/* This should never happen */
	if (unlikely(!client)) {
		pr_err("acrn-ioreq: pass the NULL parameter\n");
		return 0;
	}

	/* add refcnt for client */
	refcount_inc(&client->refcnt);

	while (!kthread_should_stop()) {
		if (has_pending_request(client)) {
			if (client->handler) {
				ret = client->handler(client->id,
					client->ioreqs_map,
					client->client_priv);
				if (ret < 0) {
					pr_err("acrn-ioreq: err:%d\n", ret);
					break;
				}
			} else {
				pr_err("acrn-ioreq: no ioreq handler\n");
				break;
			}
			continue;
		}
		wait_event_freezable(client->wq,
				     (has_pending_request(client) ||
				      kthread_should_stop()));
	}

	set_bit(IOREQ_CLIENT_EXIT, &client->flags);
	acrn_ioreq_put_client(client);
	return 0;
}

int acrn_ioreq_attach_client(int client_id)
{
	struct ioreq_client *client;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}
	client = acrn_ioreq_get_client(client_id);
	if (!client) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EFAULT;
	}

	if (client->acrn_create_kthread) {
		if (client->thread) {
			pr_warn("acrn-ioreq: kthread already exist for client %s\n",
				client->name);
			acrn_ioreq_put_client(client);
			return 0;
		}
		client->thread = kthread_run(ioreq_client_thread,
					     client,
					     "ict[%d]:%s",
					     client->vmid, client->name);
		if (IS_ERR_OR_NULL(client->thread)) {
			pr_err("acrn-ioreq: failed to run kthread for client %s\n",
			       client->name);
			client->thread = NULL;
			acrn_ioreq_put_client(client);
			return -ENOMEM;
		}
	} else {
		wait_event_freezable(client->wq,
				     (has_pending_request(client) ||
				      is_destroying(client)));

		if (is_destroying(client)) {
			acrn_ioreq_put_client(client);
			return 1;
		}
		acrn_ioreq_put_client(client);
	}

	return 0;
}

static void acrn_ioreq_notify_client(struct ioreq_client *client)
{
	/* if client thread is in waitqueue, wake up it */
	if (waitqueue_active(&client->wq))
		wake_up_interruptible(&client->wq);
}

static int ioreq_complete_request(unsigned short vmid, int vcpu,
				  struct acrn_request *acrn_req)
{
	bool polling_mode;

	polling_mode = acrn_req->completion_polling;
	acrn_req->client = -1;
	/* add barrier before setting the completion mode */
	mb();
	atomic_set(&acrn_req->processed, REQ_STATE_COMPLETE);
	/*
	 * In polling mode, HV will poll ioreqs' completion.
	 * Once marked the ioreq as REQ_STATE_COMPLETE, hypervisor side
	 * can poll the result and continue the IO flow. Thus, we don't
	 * need to notify hypervisor by hypercall.
	 * Please note, we need get completion_polling before set the request
	 * as complete, or we will race with hypervisor.
	 */
	if (!polling_mode) {
		if (hcall_notify_req_finish(vmid, vcpu) < 0) {
			pr_err("acrn-ioreq: notify request complete failed!\n");
			return -EFAULT;
		}
	}

	return 0;
}

static bool req_in_range(struct ioreq_range *range, struct acrn_request *req)
{
	bool ret = false;

	if (range->type == req->type) {
		switch (req->type) {
		case REQ_MMIO:
		case REQ_WP:
		{
			if (req->reqs.mmio_request.address >= range->start &&
			    (req->reqs.mmio_request.address +
			     req->reqs.mmio_request.size) <= range->end)
				ret = true;
			break;
		}
		case REQ_PORTIO: {
			if (req->reqs.pio_request.address >= range->start &&
			    (req->reqs.pio_request.address +
			     req->reqs.pio_request.size) <= range->end)
				ret = true;
			break;
		}

		default:
			ret = false;
			break;
		}
	}

	return ret;
}

static bool is_cfg_addr(struct acrn_request *req)
{
	return ((req->type == REQ_PORTIO) &&
		(req->reqs.pio_request.address == 0xcf8));
}

static bool is_cfg_data(struct acrn_request *req)
{
	return (req->type == REQ_PORTIO &&
		(req->reqs.pio_request.address >= 0xcfc &&
		 req->reqs.pio_request.address < (0xcfc + 4)));
}

#define PCI_LOWREG_MASK  0xFC   /* the low 8-bit of supported pci_reg addr.*/
#define PCI_HIGHREG_MASK 0xF00  /* the high 4-bit of supported pci_reg addr */
#define PCI_FUNCMAX	7       /* highest supported function number */
#define PCI_SLOTMAX	31      /* highest supported slot number */
#define PCI_BUSMAX	255     /* highest supported bus number */
#define CONF1_ENABLE	0x80000000ul
static int handle_cf8cfc(struct acrn_vm *vm, struct acrn_request *req, int vcpu)
{
	int req_handled = 0;
	int err = 0;

	/*XXX: like DM, assume cfg address write is size 4 */
	if (is_cfg_addr(req)) {
		if (req->reqs.pio_request.direction == REQUEST_WRITE) {
			if (req->reqs.pio_request.size == 4) {
				vm->pci_conf_addr = req->reqs.pio_request.value;
				req_handled = 1;
			}
		} else {
			if (req->reqs.pio_request.size == 4) {
				req->reqs.pio_request.value = vm->pci_conf_addr;
				req_handled = 1;
			}
		}
	} else if (is_cfg_data(req)) {
		if (!(vm->pci_conf_addr & CONF1_ENABLE)) {
			if (req->reqs.pio_request.direction == REQUEST_READ)
				req->reqs.pio_request.value = 0xffffffff;
			req_handled = 1;
		} else {
			/* pci request is same as io request at top */
			int offset = req->reqs.pio_request.address - 0xcfc;
			int pci_reg;
			u32 pci_cfg_addr;

			req->type = REQ_PCICFG;
			pci_cfg_addr = vm->pci_conf_addr;
			req->reqs.pci_request.bus = (pci_cfg_addr >> 16) &
						     PCI_BUSMAX;
			req->reqs.pci_request.dev = (pci_cfg_addr >> 11) &
						     PCI_SLOTMAX;
			req->reqs.pci_request.func = (pci_cfg_addr >> 8) &
						      PCI_FUNCMAX;
			pci_reg = (pci_cfg_addr & PCI_LOWREG_MASK) +
				   ((pci_cfg_addr >> 16) & PCI_HIGHREG_MASK);
			req->reqs.pci_request.reg = pci_reg + offset;
		}
	}

	if (req_handled)
		err = ioreq_complete_request(vm->vmid, vcpu, req);

	return err ? err : req_handled;
}

static
struct ioreq_client *find_ioreq_client_by_request(struct acrn_vm *vm,
						  struct acrn_request *req)
{
	struct list_head *pos, *range_pos;
	struct ioreq_client *client;
	int target_client, fallback_client;
	struct ioreq_range *range;
	bool found = false;

	target_client = 0;
	fallback_client = 0;
	spin_lock_bh(&vm->ioreq_client_lock);
	list_for_each(pos, &vm->ioreq_client_list) {
		client = container_of(pos, struct ioreq_client, list);

		if (client->fallback) {
			fallback_client = client->id;
			continue;
		}

		read_lock_bh(&client->range_lock);
		list_for_each(range_pos, &client->range_list) {
			range =
			container_of(range_pos, struct ioreq_range, list);
			if (req_in_range(range, req)) {
				found = true;
				target_client = client->id;
				break;
			}
		}
		read_unlock_bh(&client->range_lock);

		if (found)
			break;
	}
	spin_unlock_bh(&vm->ioreq_client_lock);

	if (target_client > 0)
		return acrn_ioreq_get_client(target_client);

	if (fallback_client > 0)
		return acrn_ioreq_get_client(fallback_client);

	return NULL;
}

int acrn_ioreq_distribute_request(struct acrn_vm *vm)
{
	struct acrn_request *req;
	struct list_head *pos;
	struct ioreq_client *client;
	int i, vcpu_num;

	vcpu_num = atomic_read(&vm->vcpu_num);
	for (i = 0; i < vcpu_num; i++) {
		req = vm->req_buf->req_queue + i;

		/* This function is called in tasklet only on SOS. Thus it
		 * is safe to read the state first and update it later as
		 * long as the update is atomic.
		 */
		if (atomic_read(&req->processed) == REQ_STATE_PENDING) {
			if (handle_cf8cfc(vm, req, i))
				continue;
			client = find_ioreq_client_by_request(vm, req);
			if (!client) {
				pr_err("acrn-ioreq: failed to find ioreq client\n");
				return -EINVAL;
			}
			req->client = client->id;
			/* Use the Non-RWM to assure that it is done before
			 * setting the req->processed field
			 */
			atomic_set_release(&req->processed,
					   REQ_STATE_PROCESSING);
			set_bit(i, client->ioreqs_map);
			acrn_ioreq_put_client(client);
		}
	}

	spin_lock_bh(&vm->ioreq_client_lock);
	list_for_each(pos, &vm->ioreq_client_list) {
		client = container_of(pos, struct ioreq_client, list);
		if (has_pending_request(client))
			acrn_ioreq_notify_client(client);
	}
	spin_unlock_bh(&vm->ioreq_client_lock);

	return 0;
}

int acrn_ioreq_complete_request(int client_id, uint64_t vcpu,
				struct acrn_request *acrn_req)
{
	struct ioreq_client *client;
	int ret;

	if (client_id < 0 || client_id >= MAX_CLIENT) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EINVAL;
	}
	client = acrn_ioreq_get_client(client_id);
	if (!client) {
		pr_err("acrn-ioreq: no client for id %d\n", client_id);
		return -EINVAL;
	}

	clear_bit(vcpu, client->ioreqs_map);
	if (!acrn_req) {
		acrn_req = acrn_ioreq_get_reqbuf(client_id);
		if (!acrn_req) {
			acrn_ioreq_put_client(client);
			return -EINVAL;
		}
		acrn_req += vcpu;
	}

	ret = ioreq_complete_request(client->vmid, vcpu, acrn_req);
	acrn_ioreq_put_client(client);

	return ret;
}

unsigned int acrn_dev_poll(struct file *filep, poll_table *wait)
{
	struct acrn_vm *vm = filep->private_data;
	struct ioreq_client *fallback_client;
	unsigned int ret = 0;

	if (!vm || !vm->req_buf ||
	    (vm->ioreq_fallback_client <= 0)) {
		pr_err("acrn: invalid VM !\n");
		ret = POLLERR;
		return ret;
	}

	fallback_client = acrn_ioreq_get_client(vm->ioreq_fallback_client);
	if (!fallback_client) {
		pr_err("acrn-ioreq: no client for id %d\n",
		       vm->ioreq_fallback_client);
		return -EINVAL;
	}

	poll_wait(filep, &fallback_client->wq, wait);
	if (has_pending_request(fallback_client) ||
	    is_destroying(fallback_client))
		ret = POLLIN | POLLRDNORM;

	acrn_ioreq_put_client(fallback_client);

	return ret;
}

int acrn_ioreq_init(struct acrn_vm *vm, unsigned long vma)
{
	struct acrn_set_ioreq_buffer *set_buffer;
	struct page *page;
	int ret;

	if (vm->req_buf)
		return -EEXIST;

	set_buffer = kmalloc(sizeof(*set_buffer), GFP_KERNEL);
	if (!set_buffer)
		return -ENOMEM;

	ret = get_user_pages_fast(vma, 1, 1, &page);
	if (unlikely(ret != 1) || !page) {
		pr_err("acrn-ioreq: failed to pin request buffer!\n");
		kfree(set_buffer);
		return -ENOMEM;
	}

	vm->req_buf = page_address(page);
	vm->pg = page;

	set_buffer->req_buf = page_to_phys(page);

	ret = hcall_set_ioreq_buffer(vm->vmid, virt_to_phys(set_buffer));
	kfree(set_buffer);
	if (ret < 0) {
		pr_err("acrn-ioreq: failed to set request buffer !\n");
		return -EFAULT;
	}

	pr_debug("acrn-ioreq: init request buffer @ %p!\n",
		 vm->req_buf);

	return 0;
}

void acrn_ioreq_free(struct acrn_vm *vm)
{
	struct list_head *pos, *tmp;

	/* When acrn_ioreq_destroy_client is called, it will be released
	 * and removed from vm->ioreq_client_list.
	 * The below is used to assure that the client is still released
	 * even when it is not called.
	 */
	if (!test_and_set_bit(ACRN_VM_IOREQ_FREE, &vm->flags)) {
		get_vm(vm);
		list_for_each_safe(pos, tmp, &vm->ioreq_client_list) {
			struct ioreq_client *client =
				container_of(pos, struct ioreq_client, list);
			acrn_ioreq_destroy_client(client->id);
		}
		put_vm(vm);
	}
}

void acrn_ioreq_driver_init(void)
{
	idr_init(&idr_client);
}
