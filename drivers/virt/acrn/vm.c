// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * ACRN_HSM: VM management
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Jason Chen CJ <jason.cj.chen@intel.com>
 *	Yakui Zhao <yakui.zhao@intel.com>
 */
#define pr_fmt(fmt)	"acrn: " fmt

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include "acrn_drv.h"

LIST_HEAD(acrn_vm_list);
DEFINE_RWLOCK(acrn_vm_list_lock);

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
			       struct acrn_create_vm *vm_param)
{
	int ret;

	ret = hcall_create_vm(virt_to_phys(vm_param));
	if (ret < 0 || vm_param->vmid == ACRN_INVALID_VMID) {
		pr_err("Failed to create VM by hypervisor!\n");
		return NULL;
	}

	mutex_init(&vm->regions_mapping_lock);
	vm->vmid = vm_param->vmid;
	vm->vcpu_num = vm_param->vcpu_num;

	write_lock_bh(&acrn_vm_list_lock);
	list_add(&vm->list, &acrn_vm_list);
	write_unlock_bh(&acrn_vm_list_lock);

	pr_debug("VM %d created.\n", vm->vmid);
	return vm;
}

int acrn_vm_destroy(struct acrn_vm *vm)
{
	int ret;

	/* Invalid VM */
	if (vm->vmid == ACRN_INVALID_VMID ||
	    test_and_set_bit(ACRN_VM_FLAG_DESTROYED, &vm->flags))
		return 0;

	/* Remove from global VM list */
	write_lock_bh(&acrn_vm_list_lock);
	list_del_init(&vm->list);
	write_unlock_bh(&acrn_vm_list_lock);

	acrn_unmap_vm_all_ram(vm);

	ret = hcall_destroy_vm(vm->vmid);
	if (ret < 0) {
		pr_err("Failed to destroy VM %d\n", vm->vmid);
		clear_bit(ACRN_VM_FLAG_DESTROYED, &vm->flags);
		return ret;
	}
	pr_debug("VM %d destroyed.\n", vm->vmid);
	vm->vmid = ACRN_INVALID_VMID;
	return 0;
}
