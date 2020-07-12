// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * ACRN HSM: irqfd
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Shuo Liu <shuo.a.liu@intel.com>
 *	Yakui Zhao <yakui.zhao@intel.com>
 */
#define pr_fmt(fmt) "acrn: " fmt

#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>

#include "acrn_drv.h"

static LIST_HEAD(acrn_irqfd_clients);
static DEFINE_MUTEX(acrn_irqfds_mutex);

/**
 * struct acrn_hsm_irqfd - Private structure of irqfd
 * @vm: The VM pointer
 * @wait: waitqueue node
 * @shutdown: Async shutdown work
 * @eventfd: eventfd of irqfd
 * @list: List node
 * @pt: poll_table
 * @msi: MSI data
 */
struct acrn_hsm_irqfd {
	struct acrn_vm		*vm;
	wait_queue_entry_t	wait;
	struct work_struct	shutdown;
	struct eventfd_ctx	*eventfd;
	struct list_head	list;
	poll_table		pt;
	struct acrn_msi_entry	msi;
};

static void acrn_irqfd_inject(struct acrn_hsm_irqfd *irqfd)
{
	struct acrn_vm *vm = irqfd->vm;

	acrn_inject_msi(vm->vmid, irqfd->msi.msi_addr,
			irqfd->msi.msi_data);
}

static void hsm_irqfd_shutdown(struct acrn_hsm_irqfd *irqfd)
{
	u64 cnt;

	lockdep_assert_held(&vm->irqfds_lock);

	/* remove from wait queue */
	list_del_init(&irqfd->list);
	eventfd_ctx_remove_wait_queue(irqfd->eventfd, &irqfd->wait, &cnt);
	eventfd_ctx_put(irqfd->eventfd);
	kfree(irqfd);
}

static void hsm_irqfd_shutdown_work(struct work_struct *work)
{
	struct acrn_hsm_irqfd *irqfd;
	struct acrn_vm *vm;

	irqfd = container_of(work, struct acrn_hsm_irqfd, shutdown);
	vm = irqfd->vm;
	mutex_lock(&vm->irqfds_lock);
	if (!list_empty(&irqfd->list))
		hsm_irqfd_shutdown(irqfd);
	mutex_unlock(&vm->irqfds_lock);
}

/* Called with wqh->lock held and interrupts disabled */
static int hsm_irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode,
			    int sync, void *key)
{
	unsigned long poll_bits = (unsigned long)key;
	struct acrn_hsm_irqfd *irqfd;
	struct acrn_vm *vm;

	irqfd = container_of(wait, struct acrn_hsm_irqfd, wait);
	vm = irqfd->vm;
	if (poll_bits & POLLIN)
		/* An event has been signaled, inject an interrupt */
		acrn_irqfd_inject(irqfd);

	if (poll_bits & POLLHUP)
		/* Do shutdown work in thread to hold wqh->lock */
		queue_work(vm->irqfd_wq, &irqfd->shutdown);

	return 0;
}

static void hsm_irqfd_poll_func(struct file *file, wait_queue_head_t *wqh,
				poll_table *pt)
{
	struct acrn_hsm_irqfd *irqfd;

	irqfd = container_of(pt, struct acrn_hsm_irqfd, pt);
	add_wait_queue(wqh, &irqfd->wait);
}

static int acrn_irqfd_assign(struct acrn_vm *vm, struct acrn_irqfd *args)
{
	struct eventfd_ctx *eventfd = NULL;
	struct acrn_hsm_irqfd *irqfd, *tmp;
	unsigned int events;
	struct fd f;
	int ret = 0;

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL);
	if (!irqfd)
		return -ENOMEM;

	irqfd->vm = vm;
	memcpy(&irqfd->msi, &args->msi, sizeof(args->msi));
	INIT_LIST_HEAD(&irqfd->list);
	INIT_WORK(&irqfd->shutdown, hsm_irqfd_shutdown_work);

	f = fdget(args->fd);
	if (!f.file) {
		ret = -EBADF;
		goto out;
	}

	eventfd = eventfd_ctx_fileget(f.file);
	if (IS_ERR(eventfd)) {
		ret = PTR_ERR(eventfd);
		goto fail;
	}

	irqfd->eventfd = eventfd;

	/*
	 * Install our own custom wake-up handling so we are notified via
	 * a callback whenever someone signals the underlying eventfd
	 */
	init_waitqueue_func_entry(&irqfd->wait, hsm_irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, hsm_irqfd_poll_func);

	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry(tmp, &vm->irqfds, list) {
		if (irqfd->eventfd != tmp->eventfd)
			continue;
		ret = -EBUSY;
		mutex_unlock(&vm->irqfds_lock);
		goto fail;
	}
	list_add_tail(&irqfd->list, &vm->irqfds);
	mutex_unlock(&vm->irqfds_lock);

	/* Check the pending event in this stage */
	events = f.file->f_op->poll(f.file, &irqfd->pt);

	if (events & POLLIN)
		acrn_irqfd_inject(irqfd);

	fdput(f);
	return 0;
fail:
	if (eventfd && !IS_ERR(eventfd))
		eventfd_ctx_put(eventfd);

	fdput(f);
out:
	kfree(irqfd);
	return ret;
}

static int acrn_irqfd_deassign(struct acrn_vm *vm,
			       struct acrn_irqfd *args)
{
	struct acrn_hsm_irqfd *irqfd, *tmp;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry_safe(irqfd, tmp, &vm->irqfds, list) {
		if (irqfd->eventfd == eventfd) {
			hsm_irqfd_shutdown(irqfd);
			break;
		}
	}
	mutex_unlock(&vm->irqfds_lock);
	eventfd_ctx_put(eventfd);

	return 0;
}

/**
 * acrn_irqfd_config - Configure a acrn_hsm_irqfd
 *
 * @vm: the User VM
 * @args: acrn_irqfd structure for configuring the acrn_hsm_irqfd
 *
 * Return: 0 on success, <0 on failures
 */
int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args)
{
	int ret;

	if (args->flags & ACRN_IRQFD_FLAG_DEASSIGN)
		ret = acrn_irqfd_deassign(vm, args);
	else
		ret = acrn_irqfd_assign(vm, args);

	return ret;
}

/**
 * acrn_irqfd_init - Init routine of irqfd support of ACRN HSM
 *
 * @vm: the User VM
 *
 * Return: 0 on success, <0 on failures
 */
int acrn_irqfd_init(struct acrn_vm *vm)
{
	INIT_LIST_HEAD(&vm->irqfds);
	mutex_init(&vm->irqfds_lock);
	vm->irqfd_wq = alloc_workqueue("acrn_irqfd-%d", 0, 0, vm->vmid);
	if (!vm->irqfd_wq)
		return -ENOMEM;

	pr_debug("VM %d irqfd init.\n", vm->vmid);
	return 0;
}

/**
 * acrn_irqfd_deinit - De-init routine of irqfd support of ACRN HSM
 *
 * @vm: the User VM
 *
 * Return: void
 */
void acrn_irqfd_deinit(struct acrn_vm *vm)
{
	struct acrn_hsm_irqfd *irqfd, *next;

	pr_debug("VM %d irqfd deinit.\n", vm->vmid);
	destroy_workqueue(vm->irqfd_wq);
	mutex_lock(&vm->irqfds_lock);
	list_for_each_entry_safe(irqfd, next, &vm->irqfds, list)
		hsm_irqfd_shutdown(irqfd);
	mutex_unlock(&vm->irqfds_lock);
}
