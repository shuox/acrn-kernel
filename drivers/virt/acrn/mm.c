// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * ACRN: Memory map management
 *
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Fei Li <lei1.li@intel.com>
 *	Shuo A Liu <shuo.a.liu@intel.com>
 */

#define pr_fmt(fmt) "acrn: " fmt

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include "acrn_drv.h"

static int modify_region(u16 vmid, struct vm_memory_region *region)
{
	struct vm_memory_region_list *regions;
	int ret;

	regions = kzalloc(sizeof(*regions), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	regions->vmid = vmid;
	regions->regions_num = 1;
	regions->regions_gpa = virt_to_phys(region);

	ret = hcall_set_memory_regions(virt_to_phys(regions));
	if (ret < 0)
		pr_err("Failed to set memory region for VM[%d]!\n", vmid);

	kfree(regions);
	return ret;
}

/**
 * acrn_mm_add_region() - Set up the EPT mapping of a memory region.
 * @vmid: The ID of User VM.
 * @user_gpa: A GPA of User VM.
 * @service_gpa: A GPA of Service VM.
 * @size: Size of the region.
 * @mem_type: Combination of ACRN_MEM_TYPE_*.
 * @mem_access_right: Combination of ACRN_MEM_ACCESS_*.
 *
 * Return: 0 on success, <0 on error.
 */
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa,
		       u64 size, u32 mem_type, u32 mem_access_right)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = ACRN_MEM_REGION_ADD;
	region->user_vm_pa = user_gpa;
	region->service_vm_pa = service_gpa;
	region->size = size;
	region->attr = ((mem_type & ACRN_MEM_TYPE_MASK) |
			(mem_access_right & ACRN_MEM_ACCESS_RIGHT_MASK));
	ret = modify_region(vmid, region);

	pr_debug("%s: user-GPA[%llx] service-GPA[%llx] size[0x%llx].\n",
			__func__, user_gpa, service_gpa, size);
	kfree(region);
	return ret;
}

/**
 * acrn_mm_del_region() - Del the EPT mapping of a memory region.
 * @vmid: The ID of User VM.
 * @user_gpa: A GPA of the User VM.
 * @size: Size of the region.
 *
 * Return: 0 on success, <0 for error.
 */
int acrn_mm_del_region(u16 vmid, u64 user_gpa, u64 size)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = ACRN_MEM_REGION_DEL;
	region->user_vm_pa = user_gpa;
	region->service_vm_pa = 0UL;
	region->size = size;
	region->attr = 0U;

	ret = modify_region(vmid, region);

	pr_debug("%s: user-GPA[%llx] size[0x%llx].\n",
			__func__, user_gpa, size);
	kfree(region);
	return ret;
}

int acrn_map_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	int ret;

	if (memmap->type == ACRN_MEMMAP_RAM)
		return acrn_map_vm_ram(vm, memmap);

	if (memmap->type != ACRN_MEMMAP_MMIO) {
		pr_err("Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	ret = acrn_mm_add_region(vm->vmid, memmap->user_vm_pa,
				 memmap->service_vm_pa, memmap->len,
				 ACRN_MEM_TYPE_UC, memmap->attr);
	if (ret < 0)
		pr_err("Add memory region failed, VM[%d]!\n", vm->vmid);

	return ret;
}

int acrn_unmap_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	int ret;

	if (memmap->type != ACRN_MEMMAP_MMIO) {
		pr_err("Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	ret = acrn_mm_del_region(vm->vmid, memmap->user_vm_pa, memmap->len);
	if (ret < 0)
		pr_err("Del memory region failed, VM[%d]!\n", vm->vmid);

	return ret;
}

/**
 * acrn_map_vm_ram() - Create a RAM EPT mapping of User VM.
 * @vm: The User VM
 * @memmap: Data of the EPT mapping
 *
 * Return: 0 on success, <0 for error.
 */
int acrn_map_vm_ram(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	int nr_pages, i = 0, order, nr_regions = 0;
	struct vm_memory_region_list *regions_info;
	struct vm_memory_mapping *region_mapping;
	struct vm_memory_region *vm_region;
	struct page **pages = NULL, *page;
	uint64_t user_vm_pa;
	void *remap_vaddr;
	int ret, pinned;

	if (!vm || !memmap)
		return -EINVAL;

	/* Get the page number of the map region */
	nr_pages = memmap->len >> PAGE_SHIFT;
	pages = vzalloc(nr_pages * sizeof(struct page *));
	if (!pages)
		return -ENOMEM;

	/* Lock the pages of user memory map region */
	pinned = get_user_pages_fast(memmap->vma_base,
				     nr_pages, FOLL_WRITE, pages);
	if (pinned < 0) {
		ret = pinned;
		goto free_pages;
	} else if (pinned != nr_pages) {
		ret = -EFAULT;
		goto put_pages;
	}

	/* Create a kernel map for the map region */
	remap_vaddr = vm_map_ram(pages, nr_pages, -1);
	if (!remap_vaddr) {
		ret = -ENOMEM;
		goto put_pages;
	}

	/* Record Service VM va <-> User VM pa mapping */
	mutex_lock(&vm->regions_mapping_lock);
	region_mapping = &vm->regions_mapping[vm->regions_mapping_count++];
	if (vm->regions_mapping_count < ACRN_MEM_MAPPING_MAX) {
		region_mapping->pages = pages;
		region_mapping->npages = nr_pages;
		region_mapping->size = memmap->len;
		region_mapping->service_vm_va = remap_vaddr;
		region_mapping->user_vm_pa = memmap->user_vm_pa;
	} else {
		pr_warn("Run out of memory mapping slots!\n");
	}
	mutex_unlock(&vm->regions_mapping_lock);

	/* Calculate count of vm_memory_region */
	while (i < nr_pages) {
		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		nr_regions++;
		i += 1 << order;
	}

	/* Prepare the vm_memory_region_list */
	regions_info = kzalloc(sizeof(struct vm_memory_region_list) +
			       sizeof(struct vm_memory_region) * nr_regions,
			       GFP_KERNEL);
	if (!regions_info) {
		ret = -ENOMEM;
		goto unmap_kernel_map;
	}

	/* Fill each vm_memory_region */
	vm_region = (struct vm_memory_region *)(regions_info + 1);
	regions_info->vmid = vm->vmid;
	regions_info->regions_num = nr_regions;
	regions_info->regions_gpa = virt_to_phys(vm_region);
	user_vm_pa = memmap->user_vm_pa;
	i = 0;
	while (i < nr_pages) {
		u32 region_size;

		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		region_size = PAGE_SIZE << order;
		vm_region->type = ACRN_MEM_REGION_ADD;
		vm_region->user_vm_pa = user_vm_pa;
		vm_region->service_vm_pa = page_to_phys(page);
		vm_region->size = region_size;
		vm_region->attr = (ACRN_MEM_TYPE_WB & ACRN_MEM_TYPE_MASK) |
				  (memmap->attr & ACRN_MEM_ACCESS_RIGHT_MASK);

		vm_region++;
		user_vm_pa += region_size;
		i += 1 << order;
	}

	/* Hypercall to the ACRN Hypervisor to set up EPT mappings */
	ret = hcall_set_memory_regions(virt_to_phys(regions_info));
	if (ret < 0) {
		pr_err("Failed to set regions, VM[%d]!\n", vm->vmid);
		goto unset_region;
	}
	kfree(regions_info);

	pr_debug("%s: VM[%d] SVA[%pK] GPA[%llx] size[0x%llx]\n", __func__,
		 vm->vmid, remap_vaddr, memmap->user_vm_pa, memmap->len);
	return ret;

unset_region:
	kfree(regions_info);
unmap_kernel_map:
	mutex_lock(&vm->regions_mapping_lock);
	vm->regions_mapping_count--;
	mutex_unlock(&vm->regions_mapping_lock);
	vm_unmap_ram(remap_vaddr, nr_pages);
put_pages:
	for (i = 0; i < pinned; i++)
		put_page(pages[i]);
free_pages:
	vfree(pages);
	return ret;
}

/**
 * acrn_unmap_vm_all_ram() - Destroy a RAM EPT mapping of User VM.
 * @vm: The User VM
 */
void acrn_unmap_vm_all_ram(struct acrn_vm *vm)
{
	struct vm_memory_mapping *region_mapping;
	int i, j;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region_mapping = &vm->regions_mapping[i];
		vm_unmap_ram(region_mapping->service_vm_va,
			     region_mapping->npages);
		for (j = 0; j < region_mapping->npages; j++)
			put_page(region_mapping->pages[j]);
		vfree(region_mapping->pages);
	}
	mutex_unlock(&vm->regions_mapping_lock);
}
