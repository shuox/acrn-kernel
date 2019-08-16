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
#include <linux/rwlock_types.h>
#include <linux/acrn/acrn_ioctl_defs.h>

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
	refcount_inc_checked(&vm->refcnt);
}

void put_vm(struct acrn_vm *vm)
{
	if (refcount_dec_and_test(&vm->refcnt)) {
		free_guest_mem(vm);
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

	ret = hcall_destroy_vm(vm->vmid);
	if (ret < 0) {
		pr_warn("failed to destroy VM %d\n", vm->vmid);
		clear_bit(ACRN_VM_DESTROYED, &vm->flags);
		return -EFAULT;
	}

	vm->vmid = ACRN_INVALID_VMID;
	return 0;
}
