// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN_HSM: vm management

 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 * Liu Shuo <shuo.a.liu@intel.com>
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/rwlock_types.h>
#include <linux/acrn/acrn_ioctl_defs.h>
#include <linux/acrn/acrn_drv.h>

#include "acrn_hypercall.h"
#include "acrn_drv_internal.h"

LIST_HEAD(acrn_vm_list);
DEFINE_RWLOCK(acrn_vm_list_lock);

struct acrn_vm *find_get_vm(unsigned short vmid)
{
	struct acrn_vm *vm;

	read_lock_bh(&acrn_vm_list_lock);
	list_for_each_entry(vm, &acrn_vm_list, list) {
		if (vm->vmid == vmid) {
			refcount_inc(&vm->refcnt);
			read_unlock_bh(&acrn_vm_list_lock);
			return vm;
		}
	}
	read_unlock_bh(&acrn_vm_list_lock);
	return NULL;
}

void get_vm(struct acrn_vm *vm)
{
	refcount_inc(&vm->refcnt);
}

void put_vm(struct acrn_vm *vm)
{
	if (refcount_dec_and_test(&vm->refcnt)) {
		unmap_guest_all_ram(vm);
		if (vm->monitor_page) {
			put_page(vm->monitor_page);
			vm->monitor_page = NULL;
		}
		if (vm->req_buf && vm->pg) {
			put_page(vm->pg);
			vm->pg = NULL;
			vm->req_buf = NULL;
		}
		kfree(vm);
		pr_debug("hsm: freed vm\n");
	}
}

void vm_list_add(struct list_head *list)
{
	list_add(list, &acrn_vm_list);
}

int acrn_vm_destroy(struct acrn_vm *vm)
{
	int ret;

	if (test_and_set_bit(ACRN_VM_DESTROYED, &vm->flags))
		return 0;

	acrn_ioeventfd_deinit(vm->vmid);
	acrn_irqfd_deinit(vm->vmid);
	ret = hcall_destroy_vm(vm->vmid);
	if (ret < 0) {
		pr_warn("failed to destroy VM %d\n", vm->vmid);
		clear_bit(ACRN_VM_DESTROYED, &vm->flags);
		return -EFAULT;
	}
	vm->vmid = ACRN_INVALID_VMID;
	return 0;
}

int acrn_inject_msi(unsigned short vmid, unsigned long msi_addr,
		    unsigned long msi_data)
{
	struct acrn_msi_entry *msi;
	int ret;

	/* acrn_inject_msi is called in acrn_irqfd_inject from eventfd_signal
	 * and the interrupt is disabled.
	 * So the GFP_ATOMIC should be used instead of GFP_KERNEL to
	 * avoid the sleeping with interrupt disabled.
	 */
	msi = kzalloc(sizeof(*msi), GFP_ATOMIC);
	if (!msi)
		return -ENOMEM;

	/* msi_addr: addr[19:12] with dest vcpu id */
	/* msi_data: data[7:0] with vector */
	msi->msi_addr = msi_addr;
	msi->msi_data = msi_data;
	ret = hcall_inject_msi(vmid, virt_to_phys(msi));
	kfree(msi);
	if (ret < 0) {
		pr_err("acrn: failed to inject MSI for vmid %d, msi_addr %lx msi_data%lx!\n",
		       vmid, msi_addr, msi_data);
		return -EFAULT;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(acrn_inject_msi);
