// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN_HSM: Handle IO requests
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Jason Chen CJ <jason.cj.chen@intel.com>
 * 		Zhao Yakui <yakui.zhao@intel.com>
 * 		Jack Ren <jack.ren@intel.com>
 * 		FengWei Yin <fengwei.yin@intel.com>
 * 		Shuo A Liu <shuo.a.liu@intel.com>
 */
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/acrn.h>
#include "acrn_drv.h"

static struct tasklet_struct ioreq_tasklet;

static inline bool has_pending_request(struct ioreq_client *client)
{
	return !bitmap_empty(client->ioreqs_map, ACRN_REQUEST_MAX);
}

static inline bool is_destroying(struct ioreq_client *client)
{
	return test_bit(IOREQ_CLIENT_DESTROYING, &client->flags);
}

static int ioreq_complete_request(unsigned short vmid, int vcpu,
				  struct acrn_request *acrn_req)
{
	bool polling_mode;

	polling_mode = acrn_req->completion_polling;
	/* add barrier before setting the completion mode */
	mb();
	atomic_set(&acrn_req->processed, REQ_STATE_COMPLETE);
	/*
	 * In polling mode, hypervisor will poll ioreqs' completion.
	 * Once marked the ioreq as REQ_STATE_COMPLETE, hypervisor side
	 * can poll the result and continue the IO flow. Thus, we don't
	 * need to notify hypervisor by hypercall.
	 * Please note, we need get completion_polling before set the request
	 * as complete, or we will race with hypervisor.
	 */
	if (!polling_mode) {
		if (hcall_notify_req_finish(vmid, vcpu) < 0) {
			pr_err("acrn: Notify IO request finished failed!\n");
			return -EFAULT;
		}
	}

	return 0;
}

int acrn_ioreq_complete_request(struct ioreq_client *client, unsigned long vcpu,
				struct acrn_request *acrn_req)
{
	int ret;

	clear_bit(vcpu, client->ioreqs_map);
	if (!acrn_req) {
		acrn_req = (struct acrn_request *)client->vm->req_buf;
		acrn_req += vcpu;
	}

	ret = ioreq_complete_request(client->vm->vmid, vcpu, acrn_req);

	return ret;
}

/*
 * NOTE: Just add iorange entry directly, no check for the overlap
 * clients should take care themselves
 */
int acrn_ioreq_add_range(struct ioreq_client *client,
			uint32_t type, long start, long end)
{
	struct ioreq_range *range;

	if (end < start) {
		pr_err("acrn: Invalid IO range [0x%lx,0x%lx]\n", start, end);
		return -EFAULT;
	}

	range = kzalloc(sizeof(*range), GFP_KERNEL);
	if (!range)
		return -ENOMEM;

	range->type = type;
	range->start = start;
	range->end = end;

	write_lock_bh(&client->range_lock);
	list_add(&range->list, &client->range_list);
	write_unlock_bh(&client->range_lock);

	return 0;
}

int acrn_ioreq_del_range(struct ioreq_client *client,
			uint32_t type, long start, long end)
{
	struct ioreq_range *range;

	read_lock_bh(&client->range_lock);
	list_for_each_entry(range, &client->range_list, list) {
		if ((range->type == type) &&
		    (start == range->start) && (end == range->end)) {
			list_del(&range->list);
			kfree(range);
			break;
		}
	}
	read_unlock_bh(&client->range_lock);

	return 0;
}

static int ioreq_task(void *data)
{
	struct ioreq_client *client;
	unsigned long *ioreqs_map;
	struct acrn_request *req;
	int vcpu;
	int ret;

	client = (struct ioreq_client *)data;
	if (!client->handler)
		return 0;
	ioreqs_map = client->ioreqs_map;
	while (!kthread_should_stop()) {
		while (has_pending_request(client)) {
			vcpu = find_first_bit(ioreqs_map, client->vm->vcpu_num);
			req = (struct acrn_request *)&client->vm->req_buf[vcpu];
			ret = client->handler(client, req);
			if (ret < 0) {
				pr_err("acrn: IO handle failure: %d\n", ret);
				break;
			}
			acrn_ioreq_complete_request(client, vcpu, req);
		}
		acrn_ioreq_attach_client(client);
	}

	return 0;
}

void acrn_ioreq_clear_request(struct acrn_vm *vm)
{
	struct ioreq_client *client;
	bool has_pending = false;
	int retry_cnt = 10;
	unsigned long vcpu;

	/*
	 * we need to flush the current pending ioreq dispatch
	 * tasklet and finish it before clearing all ioreq of this VM.
	 * With tasklet_kill, there still be a very rare race which
	 * might lost one ioreq tasklet for other VMs. So arm one after
	 * the clearing. It's harmless.
	 */
	tasklet_schedule(&ioreq_tasklet);
	tasklet_kill(&ioreq_tasklet);
	tasklet_schedule(&ioreq_tasklet);

	/*
	 * Now, ioreq clearing only happens when do VM reset. Current
	 * implementation is waiting all ioreq clients except the default
	 * client to be handled.
	 */
	do {
		spin_lock_bh(&vm->ioreq_clients_lock);
		list_for_each_entry(client, &vm->ioreq_clients, list) {
			if (client->is_default) continue;
			has_pending = has_pending_request(client);
			if (has_pending)
				break;
		}
		spin_unlock_bh(&vm->ioreq_clients_lock);

		if (has_pending)
			schedule_timeout_interruptible(HZ / 100);
	} while (has_pending && --retry_cnt > 0);

	if (retry_cnt == 0)
		pr_warn("acrn: %s cannot flush pending request!\n",
			client->name);

	/* Clear all ioreqs belong to DM. */
	spin_lock_bh(&vm->ioreq_clients_lock);
	client = list_last_entry(&vm->ioreq_clients, struct ioreq_client, list);
	if (client->is_default) {
		vcpu = find_next_bit(client->ioreqs_map, ACRN_REQUEST_MAX, 0);
		while (vcpu < ACRN_REQUEST_MAX) {
			acrn_ioreq_complete_request(client, vcpu, NULL);
			vcpu = find_next_bit(client->ioreqs_map,
					    ACRN_REQUEST_MAX,
					    vcpu + 1);
		}
	}
	spin_unlock_bh(&vm->ioreq_clients_lock);
}

struct ioreq_client *get_default_client(struct acrn_vm *vm)
{
	struct ioreq_client *client = NULL;

	/* treat the last one as the default client */
	spin_lock_bh(&vm->ioreq_clients_lock);
	client = list_last_entry(&vm->ioreq_clients, struct ioreq_client, list);
	if (!client->is_default)
		client = NULL;
	spin_unlock_bh(&vm->ioreq_clients_lock);

	return client;
}

/*
 * There are two cases:
 * 1) userspace attach and wait on the waitqueue through ioctl, return 1
 *    to incidate the client will be destroyed.
 * 2) kernel thread attach and wait on the waitqueue. Use kthread manner.
 */
int acrn_ioreq_attach_client(struct ioreq_client *client)
{
	if (client->is_default) {
		wait_event_interruptible(client->wq,
			has_pending_request(client) || is_destroying(client));
		if (is_destroying(client))
			return 1;
	} else
		wait_event_interruptible(client->wq,
			has_pending_request(client) || kthread_should_stop());

	return 0;
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
static bool handle_cf8cfc(struct acrn_vm *vm, struct acrn_request *req, int vcpu)
{
	bool is_handled = false;
	int offset, pci_cfg_addr;
	int pci_reg;

	if (is_cfg_addr(req)) {
		WARN_ON(req->reqs.pio_request.size != 4);
		if (req->reqs.pio_request.direction == REQUEST_WRITE)
			vm->pci_conf_addr = req->reqs.pio_request.value;
		else
			req->reqs.pio_request.value = vm->pci_conf_addr;
		is_handled = true;
	} else if (is_cfg_data(req)) {
		if (!(vm->pci_conf_addr & CONF1_ENABLE)) {
			if (req->reqs.pio_request.direction == REQUEST_READ)
				req->reqs.pio_request.value = 0xffffffff;
			is_handled = true;
		} else {
			offset = req->reqs.pio_request.address - 0xcfc;

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

	if (is_handled)
		ioreq_complete_request(vm->vmid, vcpu, req);

	return is_handled;
}

static bool in_range(struct ioreq_range *range, struct acrn_request *req)
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

/* Caller need hold ioreq_clients_lock */
static struct ioreq_client *find_ioreq_client(struct acrn_vm *vm,
					struct acrn_request *req)
{
	struct ioreq_client *client, *found = NULL;
	struct ioreq_range *range;

	list_for_each_entry(client, &vm->ioreq_clients, list) {
		/* default client is the last one */
		if (client->is_default)
			return client;
		read_lock_bh(&client->range_lock);
		list_for_each_entry(range, &client->range_list, list) {
			if (in_range(range, req)) {
				found = client;
				break;
			}
		}
		read_unlock_bh(&client->range_lock);
		if (found)
			break;
	}
	return found;
}

struct ioreq_client *acrn_ioreq_create_client(struct acrn_vm *vm,
		ioreq_handler_t handler, void *priv,
		bool is_default, char *name)
{
	struct ioreq_client *client;

	if (!handler && !is_default) {
		pr_err("acrn: Cannot create non-defult client w/o handler!\n");
		return NULL;
	}
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return NULL;

	client->handler = handler;
	client->vm = vm;
	client->priv = priv;
	client->is_default = is_default;
	if (name)
		strncpy(client->name, name, sizeof(client->name) - 1);
	rwlock_init(&client->range_lock);
	INIT_LIST_HEAD(&client->range_list);
	init_waitqueue_head(&client->wq);

	client->thread = kthread_run(ioreq_task, client, "VM%d-%s",
			client->vm->vmid, client->name);
	if (IS_ERR(client->thread)) {
		kfree(client);
		return NULL;
	}

	spin_lock_bh(&vm->ioreq_clients_lock);
	if (is_default)
		/* put the default client in tail */
		list_add_tail(&client->list, &vm->ioreq_clients);
	else
		list_add(&client->list, &vm->ioreq_clients);
	spin_unlock_bh(&vm->ioreq_clients_lock);

	pr_debug("acrn: Created ioreq client %s.\n", name);
	return client;
}

void acrn_ioreq_destroy_client(struct ioreq_client *client)
{
	struct acrn_vm *vm = client->vm;
	struct ioreq_range *range, *next;

	pr_debug("acrn: Destroy ioreq client %s.\n", client->name);
	/* Flush the tasklet and its thread to ensure no more access pending */
	tasklet_disable(&ioreq_tasklet);
	set_bit(IOREQ_CLIENT_DESTROYING, &client->flags);
	if (client->is_default)
		/* If it is default client, wake up with DESTROYING flag */
		wake_up_interruptible(&client->wq);
	else
		/* Other clients, stop their threads */
		kthread_stop(client->thread);

	/* hold the lock and remove client from list */
	spin_lock_bh(&vm->ioreq_clients_lock);
	list_del(&client->list);
	spin_unlock_bh(&vm->ioreq_clients_lock);

	write_lock_bh(&client->range_lock);
	list_for_each_entry_safe(range, next, &client->range_list, list) {
		list_del(&range->list);
		kfree(range);
	}
	write_unlock_bh(&client->range_lock);
	kfree(client);

	/* schedule once after enable in case lost tasklet of other clients */
	tasklet_enable(&ioreq_tasklet);
	tasklet_schedule(&ioreq_tasklet);
}


static int acrn_ioreq_dispatch(struct acrn_vm *vm)
{
	struct acrn_request *req;
	struct ioreq_client *client;
	int i;

	for (i = 0; i < vm->vcpu_num; i++) {
		req = vm->req_buf->req_queue + i;

		/*
		 * This function is called in tasklet only on privileged VM.
		 * Thus it is safe to read the state first and update it later.
		 */
		if (atomic_read(&req->processed) == REQ_STATE_PENDING) {
			if (handle_cf8cfc(vm, req, i))
				continue;

			spin_lock_bh(&vm->ioreq_clients_lock);
			client = find_ioreq_client(vm, req);
			if (!client) {
				pr_err("acrn: Failed to find ioreq client!\n");
				spin_unlock_bh(&vm->ioreq_clients_lock);
				return -EINVAL;
			}
			atomic_set_release(&req->processed,
					REQ_STATE_PROCESSING);
			set_bit(i, client->ioreqs_map);
			wake_up_interruptible(&client->wq);
			spin_unlock_bh(&vm->ioreq_clients_lock);
		}
	}

	return 0;
}

static void ioreq_tasklet_handler(unsigned long data)
{
	struct acrn_vm *vm;

	/* Use read_lock for list_lock as already in tasklet */
	read_lock(&acrn_vm_list_lock);
	list_for_each_entry(vm, &acrn_vm_list, list) {
		if (!vm || !vm->req_buf)
			break;

		acrn_ioreq_dispatch(vm);
	}
	read_unlock(&acrn_vm_list_lock);
}

static void ioreq_intr_handler(void)
{
	tasklet_schedule(&ioreq_tasklet);
}

void acrn_setup_ioreq_intr(void)
{
	acrn_setup_intr_irq(ioreq_intr_handler);
	tasklet_init(&ioreq_tasklet, ioreq_tasklet_handler, 0);
}

int acrn_ioreq_init(struct acrn_vm *vm, unsigned long buf_vma)
{
	struct acrn_set_ioreq_buffer *set_buffer;
	struct page *page;
	int ret;

	if (vm->req_buf)
		return -EEXIST;

	set_buffer = kmalloc(sizeof(*set_buffer), GFP_KERNEL);
	if (!set_buffer)
		return -ENOMEM;

	ret = get_user_pages_fast(buf_vma, 1, 1, &page);
	if (unlikely(ret != 1) || !page) {
		pr_err("acrn: Failed to pin ioreq page!\n");
		kfree(set_buffer);
		return -ENOMEM;
	}

	vm->req_buf = page_address(page);
	vm->ioreq_page = page;
	set_buffer->req_buf = page_to_phys(page);
	ret = hcall_set_ioreq_buffer(vm->vmid, virt_to_phys(set_buffer));
	kfree(set_buffer);
	if (ret < 0) {
		pr_err("acrn: Failed to init ioreq buffer!\n");
		put_page(page);
		vm->req_buf = NULL;
		return -EFAULT;
	}

	pr_debug("acrn: Init ioreq buffer @%p!\n", vm->req_buf);
	return 0;
}

void acrn_ioreq_deinit(struct acrn_vm *vm)
{
	struct ioreq_client *client, *next;

	pr_debug("acrn: Deinit ioreq buffer @%p!\n", vm->req_buf);
	/* Destroy all clients belong to this VM */
	list_for_each_entry_safe(client, next, &vm->ioreq_clients, list)
		acrn_ioreq_destroy_client(client);

	if (vm->req_buf && vm->ioreq_page) {
		put_page(vm->ioreq_page);
		vm->req_buf = NULL;
	}
}
