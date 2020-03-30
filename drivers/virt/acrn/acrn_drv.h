/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/acrn.h>
#include <linux/types.h>

#include "hypercall.h"

#define ACRN_MEM_MAPPING_MAX	256

#define ACRN_MEM_REGION_ADD	0
#define ACRN_MEM_REGION_DEL	2
/**
 * struct vm_memory_region - Basic VM memory region setup info.
 * @type: Operation type of this memory region.
 * @attr: Memory attribute.
 * @user_vm_pa: Physical address of User VM to be mapped.
 * @service_vm_pa: Physical address of Service VM to be mapped.
 * @size: Size of this region.
 *
 * The basic info for creating EPT mapping of User VM.
 */
struct vm_memory_region {
	u32 type;
	u32 attr;
	u64 user_vm_pa;
	u64 service_vm_pa;
	u64 size;
};

/**
 * struct vm_memory_region_list - A batch of vm_memory_region.
 * @vmid: The target VM ID.
 * @reserved: Reserved.
 * @regions_num: The number of vm_memory_region.
 * @reserved1: Reserved.
 * @regions_gpa: Physical address of a vm_memory_region array.
 *
 * Hypercall HC_VM_SET_MEMORY_REGIONS uses this structure to set up multiple
 * memory regions for a User VM. A vm_memory_region_list contains multiple
 * vm_memory_region for batch processing in the ACRN hypervisor.
 */
struct vm_memory_region_list {
	u16 vmid;
	u16 reserved[3];
	u32 regions_num;
	u32 reserved1;
	u64 regions_gpa;
};

/**
 * struct vm_memory_mapping - Memory map between a User VM and the Service VM
 * @pages: Pages in Service VM kernel.
 * @npages: Number of pages.
 * @service_vm_va: Virtual address in Service VM kernel.
 * @user_vm_pa: Physical address in User VM.
 * @size: Size of this memory region.
 *
 * HSM maintains memory mappings between a User VM GPA and the Service VM
 * kernel VA for kernel based device model emulation.
 */
struct vm_memory_mapping {
	struct page **pages;
	int npages;
	void *service_vm_va;
	u64 user_vm_pa;
	size_t size;
};

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
 * @regions_mapping_lock: Lock to protect regions_mapping.
 * @regions_mapping: Memory mappings of this VM.
 * @regions_mapping_count: Exist number of memory mapping of this VM.
 */
struct acrn_vm {
	struct list_head	list;
	u16			vmid;
	int			vcpu_num;
	unsigned long		flags;

	struct mutex		 regions_mapping_lock;
	struct vm_memory_mapping regions_mapping[ACRN_MEM_MAPPING_MAX];
	int 			 regions_mapping_count;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
			       struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa,
		       u64 size, u32 mem_type, u32 mem_access_right);
int acrn_mm_del_region(u16 vmid, u64 user_gpa, u64 size);
int acrn_map_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_unmap_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_map_vm_ram(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
void acrn_unmap_vm_all_ram(struct acrn_vm *vm);

#endif /* __ACRN_HSM_DRV_H */
