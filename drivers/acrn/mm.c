// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN: memory map management for guest VM
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Jason Chen CJ <jason.cj.chen@intel.com>
 * 		Zhao Yakui <yakui.zhao@intel.com>
 * 		Li Fei <lei1.li@intel.com>
 * 		Liu Shuo A <shuo.a.liu@intel.com>
 */
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "acrn_drv.h"

static int modify_region(unsigned short vmid,
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

	ret = hcall_set_memory_regions(virt_to_phys(regions));
	if (ret < 0) {
		pr_err("acrn: Failed to set memory region for VM[%d]!\n",
		       vmid);
		ret = -EFAULT;
	}

	kfree(regions);
	return ret;
}

int acrn_mm_add_region(unsigned short vmid, unsigned long guest_pa,
			   unsigned long host_pa, unsigned long size,
			   unsigned int mem_type, unsigned int mem_access_right)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = MR_ADD;
	region->guest_vm_pa = guest_pa;
	region->host_vm_pa = host_pa;
	region->size = size;
	region->attr = ((mem_type & MEM_TYPE_MASK) |
			(mem_access_right & MEM_ACCESS_RIGHT_MASK));
	ret = modify_region(vmid, region);

	pr_debug("acrn: %s: GPA[%lx] HPA[%lx] size[0x%lx].\n",
			__func__, guest_pa, host_pa, size);
	kfree(region);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_mm_add_region);

int acrn_mm_del_region(unsigned short vmid, unsigned long guest_pa,
			   unsigned long size)
{
	struct vm_memory_region *region;
	int ret = 0;

	region = kzalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->type = MR_DEL;
	region->guest_vm_pa = guest_pa;
	region->host_vm_pa = 0;
	region->size = size;
	region->attr = 0;

	ret = modify_region(vmid, region);

	pr_debug("acrn: %s: GPA[%lx] size[0x%lx].\n",
			__func__, guest_pa, size);
	kfree(region);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_mm_del_region);

int acrn_mm_page_wp(unsigned short vmid, unsigned long guest_pa, bool enable_wp)
{
	struct wp_data *wp;
	int ret = 0;

	wp = kzalloc(sizeof(*wp), GFP_KERNEL);
	if (!wp)
		return -ENOMEM;

	wp->set = enable_wp ? 1 : 0;
	wp->gpa = guest_pa;
	ret = hcall_write_protect_page(vmid, virt_to_phys(wp));
	if (ret < 0)
		ret = -EFAULT;
	kfree(wp);
	return ret;
}
EXPORT_SYMBOL_GPL(acrn_mm_page_wp);

int map_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	/* system ram use vma to do the mapping */
	if (memmap->type == VM_MEMMAP_SYSMEM)
		return map_guest_ram(vm, memmap);

	/* mmio */
	if (memmap->type != VM_MEMMAP_MMIO) {
		pr_err("acrn: Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	if (acrn_mm_add_region(vm->vmid, memmap->guest_vm_pa,
				   memmap->host_vm_pa, memmap->len,
				   MEM_TYPE_UC, memmap->attr) < 0){
		pr_err("acrn: Failed to add memory region for VM[%d]!\n",
				vm->vmid);
		return -EFAULT;
	}

	return 0;
}

int unmap_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	/* only handle mmio */
	if (memmap->type != VM_MEMMAP_MMIO) {
		pr_err("acrn: Invalid memmap type: %d\n", memmap->type);
		return -EINVAL;
	}

	if (acrn_mm_del_region(vm->vmid,
			memmap->guest_vm_pa, memmap->len) < 0) {
		pr_err("acrn: Failed to del memory region for VM[%d]!\n",
				vm->vmid);
		return -EFAULT;
	}

	return 0;
}

void *acrn_mm_gpa2hva(struct acrn_vm *vm, u64 guest_pa, size_t size)
{
	int i;
	struct region_mapping *region;
	void *vaddr = NULL;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region = &vm->regions_mapping[i];
		if (guest_pa < region->guest_vm_pa ||
			guest_pa >= region->guest_vm_pa + region->size)
			continue;
		if (guest_pa + size > region->guest_vm_pa + region->size) {
			pr_warn("acrn: VM[%d] gpa: 0x%llx, size %lx map fail!\n",
					vm->vmid, guest_pa, size);
			break;
		}
		vaddr = region->host_vm_va + guest_pa - region->guest_vm_pa;
	}
	mutex_unlock(&vm->regions_mapping_lock);

	return vaddr;
}
EXPORT_SYMBOL_GPL(acrn_mm_gpa2hva);

int map_guest_ram(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	struct page **pages = NULL, *page;
	int nr_pages, i = 0, order, nr_regions = 0;
	struct vm_memory_region *vm_region;
	struct map_regions *map_region_data;
	struct region_mapping *region_mapping;
	uint64_t guest_vm_pa;
	void *remap_vaddr;
	int ret;

	if (!vm || !memmap)
		return -EINVAL;

	nr_pages = memmap->len >> PAGE_SHIFT;
	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	ret = get_user_pages_fast(memmap->vma_base,
			nr_pages, FOLL_WRITE, pages);
	if (unlikely(ret != nr_pages)) {
		pr_err("Failed to pin page for Guest VM!\n");
		ret = -ENOMEM;
		goto err_pin_pages;
	}

	/* record host va <-> guest pa mapping */
	remap_vaddr = vm_map_ram(pages, nr_pages, -1, PAGE_KERNEL);
	if (!remap_vaddr) {
		ret = -ENOMEM;
		goto err_remap;
	}
	mutex_lock(&vm->regions_mapping_lock);
	region_mapping = &vm->regions_mapping[vm->regions_mapping_count++];
	if (vm->regions_mapping_count < ACRN_MAX_REGION_NUM) {
		region_mapping->pages = pages;
		region_mapping->npages = nr_pages;
		region_mapping->size = memmap->len;
		region_mapping->host_vm_va = remap_vaddr;
		region_mapping->guest_vm_pa = memmap->guest_vm_pa;
	} else
		pr_warn("acrn: Run out of memory mapping slots!\n");
	mutex_unlock(&vm->regions_mapping_lock);

	/* Calculate vm_memory_region number */
	while (i < nr_pages) {
		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		nr_regions++;
		i += 1 << order;
	}
	map_region_data = kzalloc(sizeof(struct map_regions) +
			sizeof(*vm_region) * nr_regions, GFP_KERNEL);
	if (!map_region_data) {
		ret = -ENOMEM;
		goto err_map_region_data;
	}

	vm_region = (struct vm_memory_region *)(map_region_data + 1);
	map_region_data->vmid = vm->vmid;
	map_region_data->mr_num = nr_regions;
	map_region_data->regions_pa = virt_to_phys(vm_region);
	guest_vm_pa = memmap->guest_vm_pa;
	i = 0;
	while (i < nr_pages) {
		unsigned int region_size;
		page = pages[i];
		VM_BUG_ON_PAGE(PageTail(page), page);
		order = compound_order(page);
		region_size = PAGE_SIZE << order;
		/* fill each memory region into region_array */
		vm_region->type = MR_ADD;
		vm_region->guest_vm_pa = guest_vm_pa;
		vm_region->host_vm_pa = page_to_phys(page);
		vm_region->size = region_size;
		vm_region->attr = (MEM_TYPE_WB & MEM_TYPE_MASK) |
				  (memmap->attr & MEM_ACCESS_RIGHT_MASK);

		vm_region++;
		guest_vm_pa += region_size;
		i += 1 << order;
	}

	/* hypercall to ACRN to setup memory mapping */
	ret = hcall_set_memory_regions(virt_to_phys(map_region_data));
	if (ret < 0) {
		pr_err("acrn: Failed to set regions! ret=%d\n", ret);
		ret = -EFAULT;
		goto err_set_region;
	}
	kfree(map_region_data);

	pr_debug("acrn: %s: VM[%d] HVA[%p] GPA[%llx] size[0x%llx].\n", __func__,
		vm->vmid, remap_vaddr, memmap->guest_vm_pa, memmap->len);
	return ret;

err_set_region:
	kfree(map_region_data);
err_map_region_data:
	mutex_lock(&vm->regions_mapping_lock);
	vm->regions_mapping_count--;
	mutex_unlock(&vm->regions_mapping_lock);
	vm_unmap_ram(remap_vaddr, nr_pages);
err_remap:
	for (i = 0; i < nr_pages; i++)
		put_page(pages[i]);
err_pin_pages:
	kfree(pages);
	return ret;
}

void unmap_guest_all_ram(struct acrn_vm *vm)
{
	struct region_mapping *region_mapping;
	int i, j;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region_mapping = &vm->regions_mapping[i];
		vm_unmap_ram(region_mapping->host_vm_va,
				region_mapping->npages);
		for (j = 0; j < region_mapping->npages; j++) {
			put_page(region_mapping->pages[j]);
		}
		kfree(region_mapping->pages);
	}
	mutex_unlock(&vm->regions_mapping_lock);
}
