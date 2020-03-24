// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (SRV): irqfd based on eventfd
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Liu Shuo <shuo.a.liu@intel.com>
 * 		Zhao Yakui <yakui.zhao@intel.com>
 */

#include <linux/device.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/eventfd.h>
#include <linux/kernel.h>
#include <linux/async.h>
#include <linux/slab.h>

#include <linux/acrn.h>
#include <linux/acrn_host.h>

#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

static LIST_HEAD(acrn_irqfd_clients);
static DEFINE_MUTEX(acrn_irqfds_mutex);

/* instance to bind irqfds of each VM */
struct acrn_irqfd_info {
	struct list_head list;
	int refcnt;
	/* vmid of VM */
	unsigned short  vmid;
	/* workqueue for async shutdown work */
	struct workqueue_struct *wq;

	/* the lock to protect the irqfds list */
	spinlock_t irqfds_lock;
	/* irqfds in this instance */
	struct list_head irqfds;
};

/* use internally to record properties of each irqfd */
struct acrn_hsm_irqfd {
	/* acrn_irqfd_info which this irqfd belong to */
	struct acrn_irqfd_info *info;
	/* wait queue node */
	wait_queue_entry_t wait;
	/* async shutdown work */
	struct work_struct shutdown;
	/* eventfd of this irqfd */
	struct eventfd_ctx *eventfd;
	/* list to link all ioventfd together */
	struct list_head list;
	/* poll_table of this irqfd */
	poll_table pt;
	/* msi to send when this irqfd triggerd */
	struct acrn_msi_entry msi;
};

static struct acrn_irqfd_info *get_irqfd_info_by_vm(uint16_t vmid)
{
	struct acrn_irqfd_info *info = NULL;

	mutex_lock(&acrn_irqfds_mutex);
	list_for_each_entry(info, &acrn_irqfd_clients, list) {
		if (info->vmid == vmid) {
			info->refcnt++;
			mutex_unlock(&acrn_irqfds_mutex);
			return info;
		}
	}
	mutex_unlock(&acrn_irqfds_mutex);
	return NULL;
}

static void put_irqfd_info(struct acrn_irqfd_info *info)
{
	mutex_lock(&acrn_irqfds_mutex);
	info->refcnt--;
	if (info->refcnt == 0) {
		list_del(&info->list);
		kfree(info);
	}
	mutex_unlock(&acrn_irqfds_mutex);
}

static void acrn_irqfd_inject(struct acrn_hsm_irqfd *irqfd)
{
	struct acrn_irqfd_info *info = irqfd->info;

	acrn_inject_msi(info->vmid, irqfd->msi.msi_addr,
			irqfd->msi.msi_data);
}

/*
 * Try to find if the irqfd still in list info->irqfds
 *
 * assumes info->irqfds_lock is held
 */
static bool acrn_irqfd_is_active(struct acrn_irqfd_info *info,
				 struct acrn_hsm_irqfd *irqfd)
{
	struct acrn_hsm_irqfd *_irqfd;

	list_for_each_entry(_irqfd, &info->irqfds, list)
		if (_irqfd == irqfd)
			return true;

	return false;
}

/*
 * Remove irqfd and free it.
 *
 * assumes info->irqfds_lock is held
 */
static void acrn_irqfd_shutdown(struct acrn_hsm_irqfd *irqfd)
{
	u64 cnt;

	/* remove from wait queue */
	list_del_init(&irqfd->list);
	eventfd_ctx_remove_wait_queue(irqfd->eventfd, &irqfd->wait, &cnt);
	eventfd_ctx_put(irqfd->eventfd);
	kfree(irqfd);
}

static void acrn_irqfd_shutdown_work(struct work_struct *work)
{
	struct acrn_hsm_irqfd *irqfd =
		container_of(work, struct acrn_hsm_irqfd, shutdown);
	struct acrn_irqfd_info *info = irqfd->info;

	spin_lock(&info->irqfds_lock);
	if (acrn_irqfd_is_active(info, irqfd))
		acrn_irqfd_shutdown(irqfd);
	spin_unlock(&info->irqfds_lock);
}

/*
 * Called with wqh->lock held and interrupts disabled
 */
static int acrn_irqfd_wakeup(wait_queue_entry_t *wait, unsigned int mode,
			     int sync, void *key)
{
	struct acrn_hsm_irqfd *irqfd =
		container_of(wait, struct acrn_hsm_irqfd, wait);
	unsigned long poll_bits = (unsigned long)key;
	struct acrn_irqfd_info *info = irqfd->info;

	if (poll_bits & POLLIN)
		/* An event has been signaled, inject an interrupt */
		acrn_irqfd_inject(irqfd);

	if (poll_bits & POLLHUP)
		/* async close eventfd as shutdown need hold wqh->lock */
		queue_work(info->wq, &irqfd->shutdown);

	return 0;
}

static void acrn_irqfd_poll_func(struct file *file, wait_queue_head_t *wqh,
				 poll_table *pt)
{
	struct acrn_hsm_irqfd *irqfd =
		container_of(pt, struct acrn_hsm_irqfd, pt);
	add_wait_queue(wqh, &irqfd->wait);
}

static
int acrn_irqfd_assign(struct acrn_irqfd_info *info, struct acrn_irqfd *args)
{
	struct acrn_hsm_irqfd *irqfd, *tmp;
	struct fd f;
	struct eventfd_ctx *eventfd = NULL;
	int ret = 0;
	unsigned int events;

	irqfd = kzalloc(sizeof(*irqfd), GFP_KERNEL);
	if (!irqfd)
		return -ENOMEM;

	irqfd->info = info;
	memcpy(&irqfd->msi, &args->msi, sizeof(args->msi));
	INIT_LIST_HEAD(&irqfd->list);
	INIT_WORK(&irqfd->shutdown, acrn_irqfd_shutdown_work);

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
	init_waitqueue_func_entry(&irqfd->wait, acrn_irqfd_wakeup);
	init_poll_funcptr(&irqfd->pt, acrn_irqfd_poll_func);

	spin_lock(&info->irqfds_lock);

	list_for_each_entry(tmp, &info->irqfds, list) {
		if (irqfd->eventfd != tmp->eventfd)
			continue;
		/* This fd is used for another irq already. */
		ret = -EBUSY;
		spin_unlock(&info->irqfds_lock);
		goto fail;
	}
	list_add_tail(&irqfd->list, &info->irqfds);

	spin_unlock(&info->irqfds_lock);

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

static int acrn_irqfd_deassign(struct acrn_irqfd_info *info,
			       struct acrn_irqfd *args)
{
	struct acrn_hsm_irqfd *irqfd, *tmp;
	struct eventfd_ctx *eventfd;

	eventfd = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(eventfd))
		return PTR_ERR(eventfd);

	spin_lock(&info->irqfds_lock);

	list_for_each_entry_safe(irqfd, tmp, &info->irqfds, list) {
		if (irqfd->eventfd == eventfd) {
			acrn_irqfd_shutdown(irqfd);
			break;
		}
	}

	spin_unlock(&info->irqfds_lock);
	eventfd_ctx_put(eventfd);

	return 0;
}

int acrn_irqfd_config(unsigned short vmid, struct acrn_irqfd *args)
{
	struct acrn_irqfd_info *info;
	int ret;

	info = get_irqfd_info_by_vm(vmid);
	if (!info)
		return -ENOENT;

	if (args->flags & ACRN_IRQFD_FLAG_DEASSIGN)
		ret = acrn_irqfd_deassign(info, args);
	else
		ret = acrn_irqfd_assign(info, args);

	put_irqfd_info(info);
	return ret;
}

int acrn_irqfd_init(unsigned short vmid)
{
	struct acrn_irqfd_info *info;

	info = get_irqfd_info_by_vm(vmid);
	if (info) {
		put_irqfd_info(info);
		return -EEXIST;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->vmid = vmid;
	info->refcnt = 1;
	INIT_LIST_HEAD(&info->irqfds);
	spin_lock_init(&info->irqfds_lock);

	info->wq = alloc_workqueue("acrn_irqfd-%d", 0, 0, vmid);
	if (!info->wq) {
		kfree(info);
		return -ENOMEM;
	}

	mutex_lock(&acrn_irqfds_mutex);
	list_add(&info->list, &acrn_irqfd_clients);
	mutex_unlock(&acrn_irqfds_mutex);

	pr_info("ACRN HSM irqfd init done!\n");
	return 0;
}

void acrn_irqfd_deinit(uint16_t vmid)
{
	struct acrn_hsm_irqfd *irqfd, *tmp;
	struct acrn_irqfd_info *info;

	info = get_irqfd_info_by_vm(vmid);
	if (!info)
		return;

	put_irqfd_info(info);

	destroy_workqueue(info->wq);

	spin_lock(&info->irqfds_lock);
	list_for_each_entry_safe(irqfd, tmp, &info->irqfds, list)
		acrn_irqfd_shutdown(irqfd);
	spin_unlock(&info->irqfds_lock);

	/* put one more to release it */
	put_irqfd_info(info);
}
