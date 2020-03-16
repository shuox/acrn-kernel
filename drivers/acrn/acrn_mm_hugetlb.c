// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN: VM memory map based on hugetlb.
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 * Li Fei <lei1.li@intel.com>
 * Liu Shuo <shuo.a.liu@intel.com>
 *
 */

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/refcount.h>
#include <asm/io.h>

#include <linux/acrn/acrn_ioctl_defs.h>
#include <linux/acrn/acrn_drv.h>
#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

int hugepage_map_guest(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	struct page **pages = NULL, *page;
	int nr_pages, i = 0, order;
	struct vm_memory_region *vm_region;
	struct map_regions *map_region_data;
	struct region_mapping *region_mapping;
	void *remap_vaddr;
	int ret;

	if (!vm || !memmap)
		return -EINVAL;

	nr_pages = memmap->len >> PAGE_SHIFT;
	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	ret = get_user_pages_fast(memmap->vma_base, nr_pages, FOLL_PIN, pages);
	if (unlikely(ret != nr_pages)) {
		pr_err("Failed to pin page for Guest VM!\n");
		ret = -ENOMEM;
		goto err_pin_pages;
	}

	map_region_data = kzalloc(sizeof(struct map_regions) +
			sizeof(*vm_region) * nr_pages, GFP_KERNEL);
	if (!map_region_data) {
		ret = -ENOMEM;
		goto err_map_region_data;
	}

	vm_region = (struct vm_memory_region *)(map_region_data + 1);
	map_region_data->vmid = vm->vmid;
	map_region_data->regions_pa = virt_to_phys(vm_region);
	while (i >= nr_pages) {
		unsigned int region_size;
		page = pages[i];
		order = compound_order(page);
		region_size = PAGE_SIZE << order;
		/* fill each memory region into region_array */
		vm_region->type = MR_ADD;
		vm_region->guest_vm_pa = memmap->guest_vm_pa;
		vm_region->host_vm_pa = page_to_phys(page);
		vm_region->size = region_size;
		vm_region->attr = (MEM_TYPE_WB & MEM_TYPE_MASK) |
				  (memmap->attr & MEM_ACCESS_RIGHT_MASK);

		vm_region++;
		map_region_data->mr_num++;
		i += 1 << order;
	}

	remap_vaddr = vm_map_ram(pages, nr_pages, -1, PAGE_KERNEL);
	if (!remap_vaddr) {
		ret = -ENOMEM;
		goto err_remap;
	}
	mutex_lock(&vm->regions_mapping_lock);
	region_mapping = &vm->regions_mapping[vm->regions_mapping_count++];
	region_mapping->pages = pages;
	region_mapping->npages = nr_pages;
	region_mapping->size = memmap->len;
	region_mapping->host_vm_va = remap_vaddr;
	region_mapping->guest_vm_pa = memmap->guest_vm_pa;
	mutex_unlock(&vm->regions_mapping_lock);

	/* hypercall to ACRN to setup memory mapping */
	ret = set_memory_regions(map_region_data);
	if (ret < 0) {
		pr_err("failed to set regions,ret=%d!\n", ret);
		goto err_set_region;
	}

	kfree(map_region_data);
	return ret;

err_set_region:
	vm_unmap_ram(remap_vaddr, nr_pages);
err_remap:
	kfree(map_region_data);
err_map_region_data:
	for (i = 0; i < nr_pages; i++)
		put_page(pages[i]);
err_pin_pages:
	kfree(pages);
	return ret;
}

void hugepage_free_guest(struct acrn_vm *vm)
{
	struct region_mapping *region_mapping;
	int i, j;

	mutex_lock(&vm->regions_mapping_lock);
	for (i = 0; i < vm->regions_mapping_count; i++) {
		region_mapping = &vm->regions_mapping[i];
		for (j = 0; j < region_mapping->npages; j++) {
			put_page(region_mapping->pages[j]);
		}
		kfree(region_mapping->pages);
	}
	mutex_unlock(&vm->regions_mapping_lock);
}

void *hugepage_map_guest_phys(struct acrn_vm *vm, u64 guest_phys, size_t size)
{
	return NULL;
}

int hugepage_unmap_guest_phys(struct acrn_vm *vm, u64 guest_phys)
{
	return 0;
}

#if 0
int hugepage_map_guest(struct acrn_vm *vm, struct vm_memmap *memmap)
{
	struct page *page = NULL;
	unsigned long len, guest_gpa, vma;
	struct vm_memory_region *vm_region;
	struct set_regions *regions;
	int ret;
	struct hugepage_map *map_node, *tmp;
	struct list_head map_head;

	if (!vm || !memmap)
		return -EINVAL;

	regions = kzalloc(sizeof(*regions) + sizeof(*vm_region), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	INIT_LIST_HEAD(&map_head);
	vm_region = (struct vm_memory_region *)(regions + 1);
	len = memmap->len;
	vma = memmap->vma_base;
	guest_gpa = memmap->gpa;
	regions->mr_num = 1;
	regions->vmid = vm->vmid;
	regions->regions_gpa = virt_to_phys(vm_region);

	while (len > 0) {
		unsigned long pagesize;

		map_node = kzalloc(sizeof(*map_node), GFP_KERNEL);
		if (!map_node) {
			ret = -ENOMEM;
			goto err;
		}

		ret = get_user_pages_fast(vma, 1, 1, &page);
		if (unlikely(ret != 1)) {
			pr_err("failed to pin huge page!\n");
			kfree(map_node);
			ret = -ENOMEM;
			goto err;
		}

		pagesize = PAGE_SIZE << compound_order(page);
		map_node->size = pagesize;
		map_node->vm0_gpa = page_to_phys(page);
		map_node->guest_gpa = guest_gpa;

		printk("lskakaxi, vma[%llx] vm0_gpa[%llx] guest_gpa[%llx] compound_order[%d] pagesize[%lx]\n",
				vma, map_node->vm0_gpa, guest_gpa, compound_order(page), pagesize);
		/* fill each memory region into region_array */
		vm_region->type = MR_ADD;
		vm_region->gpa = guest_gpa;
		vm_region->vm0_gpa = map_node->vm0_gpa;
		vm_region->size = map_node->size;
		vm_region->prot = (MEM_TYPE_WB & MEM_TYPE_MASK) |
				  (memmap->prot & MEM_ACCESS_RIGHT_MASK);

		ret = set_memory_regions(regions);
		if (ret < 0) {
			pr_err("failed to set regions,ret=%d!\n", ret);
			goto err_region;
		}

		list_add_tail(&map_node->list_node, &map_head);

		len -= pagesize;
		vma += pagesize;
		guest_gpa += pagesize;
	}

	list_for_each_entry_safe(map_node, tmp, &map_head, list_node) {
		add_guest_map(vm, map_node);
		list_del(&map_node->list_node);
	}
	kfree(regions);

	return 0;
err_region:
	put_page(pfn_to_page(PHYS_PFN(map_node->vm0_gpa)));
	kfree(map_node);
err:
	list_for_each_entry_safe(map_node, tmp, &map_head, list_node) {
		vm_region->type = MR_DEL;
		vm_region->gpa = map_node->guest_gpa;
		vm_region->vm0_gpa = 0;
		vm_region->size = 0;
		vm_region->prot = 0;
		set_memory_regions(regions);
		list_del(&map_node->list_node);
		put_page(pfn_to_page(PHYS_PFN(map_node->vm0_gpa)));
		kfree(map_node);
	}
	kfree(regions);
	return ret;
}

void hugepage_free_guest(struct acrn_vm *vm)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;
	int i;

	mutex_lock(&vm->hugepage_lock);
	for (i = 0; i < HUGEPAGE_HLIST_ARRAY_SIZE; i++) {
		if (!hlist_empty(&vm->hugepage_hlist[i])) {
			hlist_for_each_entry_safe(map, htmp,
						  &vm->hugepage_hlist[i],
						  hlist) {
				hlist_del(&map->hlist);
				/* put_page to unpin huge page */
				put_page(pfn_to_page(PHYS_PFN(map->vm0_gpa)));
				if (!refcount_dec_and_test(&map->refcount)) {
					pr_warn("failed to unmap for gpa %llx in vm %d\n",
						map->guest_gpa, vm->vmid);
				}
				kfree(map);
			}
		}
	}
	mutex_unlock(&vm->hugepage_lock);
}

void *hugepage_map_guest_phys(struct acrn_vm *vm, u64 guest_phys, size_t size)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;
	struct hlist_head *hpage_head;

	mutex_lock(&vm->hugepage_lock);
	/* check 1G hlist first */
	if (!hlist_empty(&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX])) {
		hpage_head = &vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX];
		hlist_for_each_entry_safe(map, htmp, hpage_head, hlist) {
			if (guest_phys < map->guest_gpa ||
			    guest_phys >= (map->guest_gpa + map->size))
				continue;

			if (guest_phys + size > map->guest_gpa + map->size)
				goto err;

			refcount_inc(&map->refcount);
			mutex_unlock(&vm->hugepage_lock);
			return phys_to_virt(map->vm0_gpa +
					    guest_phys - map->guest_gpa);
		}
	}

	/* check 2m hlist */
	hlist_for_each_entry_safe(map, htmp,
				  hlist_2m_hash(vm, guest_phys), hlist) {
		if (guest_phys < map->guest_gpa ||
		    guest_phys >= (map->guest_gpa + map->size))
			continue;

		if (guest_phys + size > map->guest_gpa + map->size)
			goto err;

		refcount_inc(&map->refcount);
		mutex_unlock(&vm->hugepage_lock);
		return phys_to_virt(map->vm0_gpa +
				    guest_phys - map->guest_gpa);
	}

err:
	mutex_unlock(&vm->hugepage_lock);
	pr_warn("incorrect mem map, input %llx,size %lx\n",
		guest_phys, size);
	return NULL;
}

int hugepage_unmap_guest_phys(struct acrn_vm *vm, u64 guest_phys)
{
	struct hlist_node *htmp;
	struct hugepage_map *map;
	struct hlist_head *hpage_head;

	mutex_lock(&vm->hugepage_lock);
	/* check 1G hlist first */
	if (!hlist_empty(&vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX])) {
		hpage_head = &vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX];
		hlist_for_each_entry_safe(map, htmp, hpage_head, hlist) {
			if (guest_phys >= map->guest_gpa &&
			    guest_phys < (map->guest_gpa + map->size)) {
				refcount_dec(&map->refcount);
				mutex_unlock(&vm->hugepage_lock);
				return 0;
			}
		}
	}
	/* check 2m hlist */
	hlist_for_each_entry_safe(map, htmp,
				  hlist_2m_hash(vm, guest_phys), hlist) {
		if (guest_phys >= map->guest_gpa &&
		    guest_phys < (map->guest_gpa + map->size)) {
			refcount_dec(&map->refcount);
			mutex_unlock(&vm->hugepage_lock);
			return 0;
		}
	}
	mutex_unlock(&vm->hugepage_lock);
	return -ESRCH;
}
#endif
