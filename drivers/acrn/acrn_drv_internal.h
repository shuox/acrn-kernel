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

struct vm_memory_region {
#define MR_ADD		0
#define MR_DEL		2
	u32 type;

	/* IN: mem attr */
	u32 prot;

	/* IN: beginning guest GPA to map */
	u64 gpa;

	/* IN: VM0's GPA which foreign gpa will be mapped to */
	u64 vm0_gpa;

	/* IN: size of the region */
	u64 size;
};

struct set_regions {
	/*IN: vmid for this hypercall */
	u16 vmid;

	/** Reserved */
	u16 reserved[3];

	/* IN: multi memmaps numbers */
	u32 mr_num;

	/** Reserved */
	u32 reserved1;
	/* IN:
	 * the gpa of memmaps buffer, point to the memmaps array:
	 * struct memory_map memmap_array[memmaps_num]
	 * the max buffer size is one page.
	 */
	u64 regions_gpa;
};

struct wp_data {
	/** set page write protect permission.
	 *  true: set the wp; flase: clear the wp
	 */
	u8 set;

	/** Reserved */
	u8 reserved[7];

	/** the guest physical address of the page to change */
	u64 gpa;
};

#define ACRN_INVALID_VMID (-1)

enum ACRN_VM_FLAGS {
	ACRN_VM_DESTROYED = 0,
};

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;

void vm_list_add(struct list_head *list);

#define HUGEPAGE_2M_HLIST_ARRAY_SIZE	32
#define HUGEPAGE_1G_HLIST_ARRAY_SIZE	1
#define HUGEPAGE_HLIST_ARRAY_SIZE	(HUGEPAGE_2M_HLIST_ARRAY_SIZE + \
					 HUGEPAGE_1G_HLIST_ARRAY_SIZE)
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
 * @hugepage_hlist: hash list of hugepage
 */
struct acrn_vm {
	struct device *dev;
	struct list_head list;
	unsigned short vmid;
	refcount_t refcnt;
	int max_gfn;
	atomic_t vcpu_num;
	unsigned long flags;
	/* mutex to protect hugepage_hlist */
	struct mutex hugepage_lock;
	struct hlist_head hugepage_hlist[HUGEPAGE_HLIST_ARRAY_SIZE];
};

int acrn_vm_destroy(struct acrn_vm *vm);

struct acrn_vm *find_get_vm(unsigned short vmid);
void get_vm(struct acrn_vm *vm);
void put_vm(struct acrn_vm *vm);
void free_guest_mem(struct acrn_vm *vm);
int map_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int unmap_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int hugepage_map_guest(struct acrn_vm *vm, struct vm_memmap *memmap);
void hugepage_free_guest(struct acrn_vm *vm);
void *hugepage_map_guest_phys(struct acrn_vm *vm, u64 guest_phys, size_t size);
int hugepage_unmap_guest_phys(struct acrn_vm *vm, u64 guest_phys);
int set_memory_regions(struct set_regions *regions);
#endif
