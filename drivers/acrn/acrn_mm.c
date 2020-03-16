// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN: memory map management for each VM
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 * Li Fei <lei1.li@intel.com>
 * Liu Shuo <shuo.a.liu@intel.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <linux/acrn/acrn_ioctl_defs.h>
#include <linux/acrn/acrn_drv.h>

#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

int set_memory_regions(struct map_regions *regions)
{
	if (!regions)
		return -EINVAL;

	if (regions->mr_num > 0)
		if (hcall_set_memory_regions(virt_to_phys(regions)) < 0)
			return -EFAULT;

	return 0;
}

static int set_memory_region(unsigned short vmid,
		struct vm_memory_region *region)
{
	struct map_regions *regions;
	int ret;

	regions = kzalloc(sizeof(*regions), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	regions->vmid = vmid;
	regions->mr_num = 1;
	regions->regions_pa = virt_to_phys(region);

	ret = set_memory_regions(regions);
	kfree(regions);
	if (ret < 0) {
		pr_err("acrn: Failed to set memory region for vm[%d]!\n",
		       vmid);
		return -EFAULT;
	}

	return 0;
}

int acrn_add_memory_region(unsigned short vmid, unsigned long gpa,
			   unsigned long host_gpa, unsigned long size,
			   unsigned int mem_type, unsigned int mem_access_right)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = MR_ADD;
	region->guest_vm_pa = gpa;
	region->host_vm_pa = host_gpa;
	region->size = size;
	region->attr = ((mem_type & MEM_TYPE_MASK) |
			(mem_access_right & MEM_ACCESS_RIGHT_MASK));
	ret = set_memory_region(vmid, region);

	kfree(region);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_add_memory_region);

int acrn_del_memory_region(unsigned short vmid, unsigned long gpa,
			   unsigned long size)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = MR_DEL;
	region->guest_vm_pa = gpa;
	region->host_vm_pa = 0;
	region->size = size;
	region->attr = 0;

	ret = set_memory_region(vmid, region);

	kfree(region);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_del_memory_region);

int acrn_write_protect_page(unsigned short vmid,
			    unsigned long gpa, bool enable_wp)
{
	struct wp_data *wp;
	int ret = 0;

	wp = kzalloc(sizeof(*wp), GFP_KERNEL);
	if (!wp)
		return -ENOMEM;

	wp->set = enable_wp ? 1 : 0;
	wp->gpa = gpa;
	ret = hcall_write_protect_page(vmid, virt_to_phys(wp));
	kfree(wp);

	if (ret < 0) {
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acrn_write_protect_page);

int map_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	/* hugetlb use vma to do the mapping */
	if (memmap->type == VM_MEMMAP_SYSMEM)
		return hugepage_map_guest(vm, memmap);

	/* mmio */
	if (memmap->type != VM_MEMMAP_MMIO) {
		pr_err("acrn: %s invalid memmap type: %d\n",
		       __func__, memmap->type);
		return -EINVAL;
	}

	if (acrn_add_memory_region(vm->vmid, memmap->guest_vm_pa,
				   memmap->host_vm_pa, memmap->len,
				   MEM_TYPE_UC, memmap->attr) < 0){
		pr_err("acrn: failed to set memory region %d!\n", vm->vmid);
		return -EFAULT;
	}

	return 0;
}

int unmap_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	/* only handle mmio */
	if (memmap->type != VM_MEMMAP_MMIO) {
		pr_err("hsm: %s invalid memmap type: %d for unmap\n",
		       __func__, memmap->type);
		return -EINVAL;
	}

	if (acrn_del_memory_region(vm->vmid, memmap->guest_vm_pa, memmap->len) < 0) {
		pr_err("hsm: failed to del memory region %d!\n", vm->vmid);
		return -EFAULT;
	}

	return 0;
}

void free_guest_mem(struct acrn_vm *vm)
{
	return hugepage_free_guest(vm);
}

void *acrn_map_guest_phys(unsigned short vmid, u64 guest_phys, size_t size)
{
	struct acrn_vm *vm;
	void *ret;

	vm = find_get_vm(vmid);
	if (!vm)
		return NULL;

	ret = hugepage_map_guest_phys(vm, guest_phys, size);

	put_vm(vm);

	return ret;
}
EXPORT_SYMBOL_GPL(acrn_map_guest_phys);

int acrn_unmap_guest_phys(unsigned short vmid, u64 guest_phys)
{
	struct acrn_vm *vm;
	int ret;

	vm = find_get_vm(vmid);
	if (!vm) {
		pr_warn("vm_list corrupted\n");
		return -ESRCH;
	}

	ret = hugepage_unmap_guest_phys(vm, guest_phys);

	put_vm(vm);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_unmap_guest_phys);
