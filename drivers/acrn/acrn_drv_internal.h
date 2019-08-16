/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN_HSM : vm management header file.
 *
 */

#ifndef __ACRN_VM_MNGT_H
#define __ACRN_VM_MNGT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/refcount.h>

#define ACRN_INVALID_VMID (-1)

enum ACRN_VM_FLAGS {
	ACRN_VM_DESTROYED = 0,
};

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;

void vm_list_add(struct list_head *list);

/**
 * struct acrn_vm - data structure to track guest
 *
 * @dev: pointer to dev of linux device mode
 * @list: list of acrn_vm
 * @vmid: guest vmid
 * @refcnt: reference count of guest
 * @max_gfn: maximum guest page frame number
 * @vcpu_num: vcpu number
 * @flags: VM flag bits
 */
struct acrn_vm {
	struct device *dev;
	struct list_head list;
	unsigned short vmid;
	refcount_t refcnt;
	int max_gfn;
	atomic_t vcpu_num;
	unsigned long flags;
};

int acrn_vm_destroy(struct acrn_vm *vm);

/**
 * find_get_vm() - find and keep guest acrn_vm based on the vmid
 *
 * @vmid: guest vmid
 *
 * Return: pointer to acrn_vm, NULL if can't find vm matching vmid
 */
struct acrn_vm *find_get_vm(unsigned short vmid);

/**
 * get_vm() - increase the refcnt of acrn_vm
 * @vm: pointer to acrn_vm which identify specific guest
 *
 * Return:
 */
void get_vm(struct acrn_vm *vm);

/**
 * put_vm() - release acrn_vm of guest according to guest vmid
 * If the latest reference count drops to zero, free acrn_vm as well
 * @vm: pointer to acrn_vm which identify specific guest
 *
 * Return:
 */
void put_vm(struct acrn_vm *vm);

#endif
