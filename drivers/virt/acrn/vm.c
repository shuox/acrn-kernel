// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN_HSM: VM management
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:
 * 	Jason Chen CJ <jason.cj.chen@intel.com>
 * 	Yakui Zhao <yakui.zhao@intel.com>
 */
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mm.h>
#include "acrn_drv.h"

LIST_HEAD(acrn_vm_list);
DEFINE_RWLOCK(acrn_vm_list_lock);

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
		struct acrn_create_vm *vm_param)
{
	int ret;

	ret = hcall_create_vm(virt_to_phys(vm_param));
	if ((ret < 0) || (vm_param->vmid == ACRN_INVALID_VMID)) {
		pr_err("acrn: failed to create VM by Hypervisor!\n");
		return NULL;
	}

	mutex_init(&vm->regions_mapping_lock);
	INIT_LIST_HEAD(&vm->ioreq_clients);
	spin_lock_init(&vm->ioreq_clients_lock);
	vm->vmid = vm_param->vmid;
	vm->vcpu_num = vm_param->vcpu_num;

	if (acrn_ioreq_init(vm, vm_param->req_buf) < 0) {
		hcall_destroy_vm(vm_param->vmid);
		vm->vmid = ACRN_INVALID_VMID;
		return NULL;
	}

	write_lock_bh(&acrn_vm_list_lock);
	list_add(&vm->list, &acrn_vm_list);
	write_unlock_bh(&acrn_vm_list_lock);

	acrn_ioeventfd_init(vm);
	acrn_irqfd_init(vm);

	pr_info("acrn: VM %d created.\n", vm->vmid);
	return vm;
}

int acrn_vm_destroy(struct acrn_vm *vm)
{
	int ret;

	/* Invalid VM or has been destroyed */
	if (vm->vmid == ACRN_INVALID_VMID ||
		test_and_set_bit(ACRN_VM_DESTROYED, &vm->flags))
		return 0;

	/* Remove from global vm list */
	write_lock_bh(&acrn_vm_list_lock);
	list_del_init(&vm->list);
	write_unlock_bh(&acrn_vm_list_lock);

	acrn_ioeventfd_deinit(vm);
	acrn_irqfd_deinit(vm);
	acrn_ioreq_deinit(vm);

	unmap_guest_all_ram(vm);
	if (vm->monitor_page) {
		put_page(vm->monitor_page);
		vm->monitor_page = NULL;
	}

	ret = hcall_destroy_vm(vm->vmid);
	if (ret < 0) {
		pr_warn("acrn: Failed to destroy VM %d\n", vm->vmid);
		clear_bit(ACRN_VM_DESTROYED, &vm->flags);
		return -EFAULT;
	}
	pr_info("acrn: VM %d destroyed.\n", vm->vmid);
	vm->vmid = ACRN_INVALID_VMID;
	return 0;
}

int acrn_inject_msi(uint16_t vmid, unsigned long msi_addr,
		unsigned long msi_data)
{
	struct acrn_msi_entry *msi;
	int ret;

	/* might be used in interrupt context, so use GFP_ATOMIC */
	msi = kzalloc(sizeof(*msi), GFP_ATOMIC);
	if (!msi)
		return -ENOMEM;

	/*
	 * msi_addr: addr[19:12] with dest vcpu id
	 * msi_data: data[7:0] with vector
	 */
	msi->msi_addr = msi_addr;
	msi->msi_data = msi_data;
	ret = hcall_inject_msi(vmid, virt_to_phys(msi));
	if (ret < 0) {
		pr_err("acrn: Failed to inject MSI!\n");
		ret = -EFAULT;
	}
	kfree(msi);
	return ret;
}
