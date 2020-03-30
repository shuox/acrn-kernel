/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/acrn.h>
#include <linux/types.h>

#include "hypercall.h"

#define ACRN_INVALID_VMID (0xffffU)

#define ACRN_VM_FLAG_DESTROYED		0U
extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
/**
 * struct acrn_vm - Structure of ACRN VM.
 * @list: list node to link all VMs
 * @vmid: VM ID
 * @vcpu_num: vCPU number of the VM
 * @flags: flags of the VM
 */
struct acrn_vm {
	struct list_head	list;
	u16			vmid;
	int			vcpu_num;
	unsigned long		flags;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
			       struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);

#endif /* __ACRN_HSM_DRV_H */
