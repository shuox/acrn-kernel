// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN: Memory map management
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Fei Li <lei1.li@intel.com>
 *	Shuo A Liu <shuo.a.liu@intel.com>
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/acrn_hsm.h>

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
		pr_err("acrn: Failed to set memory region for VM[%d]!\n", vmid);

	kfree(regions);
	return ret;
}

/**
 * Set up the EPT mapping of a memory region
 *
 * @vmid: The ID of User VM
 * @user_gpa: A GPA of User VM
 * @service_gpa: A GPA of Service VM
 * @size: Size of the region
 * @mem_type: Combination of ACRN_MEM_TYPE_*
 * @mem_access_right: Combination of ACRN_MEM_ACCESS_*
 *
 * Return: 0 on success, <0 for error.
 */
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa, u64 size,
			   u32 mem_type, u32 mem_access_right)
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

	pr_debug("acrn: %s: user-GPA[%llx] service-GPA[%llx] size[0x%llx].\n",
			__func__, user_gpa, service_gpa, size);
	kfree(region);
	return ret;
}
EXPORT_SYMBOL(acrn_mm_add_region);

/**
 * Del the EPT mapping of a memory region
 *
 * @vmid: The ID of User VM
 * @user_gpa: A GPA of the User VM
 * @size: size of the region
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

	pr_debug("acrn: %s: user-GPA[%llx] size[0x%llx].\n",
			__func__, user_gpa, size);
	kfree(region);
	return ret;
}
EXPORT_SYMBOL(acrn_mm_del_region);

int acrn_map_guest_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	if (memmap->type == ACRN_MEMMAP_RAM)
		return acrn_map_guest_ram(vm, memmap);

	if (memmap->type != ACRN_MEMMAP_MMIO) {
		pr_err("acrn: Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	if (acrn_mm_add_region(vm->vmid, memmap->user_vm_pa,
				   memmap->service_vm_pa, memmap->len,
				   ACRN_MEM_TYPE_UC, memmap->attr) < 0) {
		pr_err("acrn: Add memory region failed, VM[%d]!\n", vm->vmid);
		return -EFAULT;
	}

	return 0;
}

int acrn_unmap_guest_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	if (memmap->type != ACRN_MEMMAP_MMIO) {
		pr_err("acrn: Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	if (acrn_mm_del_region(vm->vmid,
			memmap->user_vm_pa, memmap->len) < 0) {
		pr_err("acrn: Del memory region failed, VM[%d]!\n", vm->vmid);
		return -EFAULT;
	}

	return 0;
}

/*
 * Convert a User VM GPA to a Service VM GVA
 *
 * @vm: The User VM pointer
 * @user_gpa: A GPA of User VM
 * @size: range size for sanity check
 *
 * Return: Service VM kernel virtual address, NULL on error
 */
void *acrn_mm_gpa2sva(struct acrn_vm *vm, u64 user_gpa, size_t size)
{
	int i;
	struct vm_memory_mapping *region;
	void *vaddr = NULL;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region = &vm->regions_mapping[i];
		if (user_gpa < region->user_vm_pa ||
			user_gpa >= region->user_vm_pa + region->size)
			continue;
		if (user_gpa + size > region->user_vm_pa + region->size) {
			pr_warn("acrn: VM[%d] gpa:0x%llx, size %lx map fail!\n",
					vm->vmid, user_gpa, size);
			break;
		}
		vaddr = region->service_vm_va + user_gpa - region->user_vm_pa;
	}
	mutex_unlock(&vm->regions_mapping_lock);

	return vaddr;
}
EXPORT_SYMBOL(acrn_mm_gpa2sva);

/**
 * Create a RAM EPT mapping of guest VM
 *
 * @vm: The User VM pointer
 * @memmap: data pointer for the EPT mapping
 *
 * Return: 0 on success, <0 for error.
 */
int acrn_map_guest_ram(struct acrn_vm *vm, struct acrn_vm_memmap *memmap)
{
	struct page **pages = NULL, *page;
	int nr_pages, i = 0, order, nr_regions = 0;
	struct vm_memory_region *vm_region;
	struct vm_memory_region_list *regions_info;
	struct vm_memory_mapping *region_mapping;
	uint64_t user_vm_pa;
	void *remap_vaddr;
	int ret;

	if (!vm || !memmap)
		return -EINVAL;

	/* Get the page number of the map region */
	nr_pages = memmap->len >> PAGE_SHIFT;
	pages = vzalloc(nr_pages * sizeof(struct page *));
	if (!pages)
		return -ENOMEM;

	/* Lock the pages of user memory map region */
	ret = get_user_pages_fast(memmap->vma_base,
			nr_pages, FOLL_WRITE, pages);
	if (unlikely(ret != nr_pages)) {
		pr_err("acrn: Failed to pin page for User VM!\n");
		ret = -ENOMEM;
		goto err_pin_pages;
	}

	/* Create a kernel map for the map region */
	remap_vaddr = vm_map_ram(pages, nr_pages, -1);
	if (!remap_vaddr) {
		ret = -ENOMEM;
		goto err_remap;
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
	} else
		pr_warn("acrn: Run out of memory mapping slots!\n");
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
		goto err_map_region_data;
	}

	/* Fill each vm_memory_region */
	vm_region = (struct vm_memory_region *)(regions_info + 1);
	regions_info->vmid = vm->vmid;
	regions_info->regions_num = nr_regions;
	regions_info->regions_gpa = virt_to_phys(vm_region);
	user_vm_pa = memmap->user_vm_pa;
	i = 0;
	while (i < nr_pages) {
		unsigned int region_size;

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

	/* Hypercall to the ACRN hypervisor to set up EPT mappings */
	ret = hcall_set_memory_regions(virt_to_phys(regions_info));
	if (ret < 0) {
		pr_err("acrn: Failed to set regions, VM[%d]!\n", vm->vmid);
		goto err_set_region;
	}
	kfree(regions_info);

	pr_debug("acrn: %s: VM[%d] SVA[%p] GPA[%llx] size[0x%llx]\n", __func__,
		vm->vmid, remap_vaddr, memmap->user_vm_pa, memmap->len);
	return ret;

err_set_region:
	kfree(regions_info);
err_map_region_data:
	mutex_lock(&vm->regions_mapping_lock);
	vm->regions_mapping_count--;
	mutex_unlock(&vm->regions_mapping_lock);
	vm_unmap_ram(remap_vaddr, nr_pages);
err_remap:
	for (i = 0; i < nr_pages; i++)
		put_page(pages[i]);
err_pin_pages:
	vfree(pages);
	return ret;
}

/**
 * Destroy a RAM EPT mapping of guest VM
 *
 * @vm: The User VM pointer
 */
void acrn_unmap_guest_all_ram(struct acrn_vm *vm)
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
