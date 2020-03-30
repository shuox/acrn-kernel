/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/acrn.h>

#include "hypercall.h"

#define ACRN_INVALID_VMID (0xffffU)

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
struct acrn_vm {
	/* list node to link all VMs */
	struct list_head list;
	/* VM ID */
	u16	vmid;
	/* vCPU number of the VM */
	int	vcpu_num;

#define ACRN_VM_FLAG_DESTROYED		0U
	unsigned long flags;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
		struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);

#endif
