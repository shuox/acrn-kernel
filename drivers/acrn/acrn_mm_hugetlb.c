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

#include <linux/acrn/acrn_ioctl_defs.h>
#include <linux/acrn/acrn_drv.h>
#include "acrn_drv_internal.h"
#include "acrn_hypercall.h"

#define HUGEPAGE_2M_SHIFT	21
#define HUGEPAGE_1G_SHIFT	30

#define HUGEPAGE_1G_HLIST_IDX	(HUGEPAGE_HLIST_ARRAY_SIZE - 1)

struct hugepage_map {
	struct hlist_node hlist;
	/* This is added into the temporal list for failure */
	struct list_head  list_node;
	u64 vm0_gpa;
	size_t size;
	u64 guest_gpa;
	refcount_t refcount;
};

static inline
struct hlist_head *hlist_2m_hash(struct acrn_vm *vm,
				 unsigned long guest_gpa)
{
	return &vm->hugepage_hlist[guest_gpa >> HUGEPAGE_2M_SHIFT &
			(HUGEPAGE_2M_HLIST_ARRAY_SIZE - 1)];
}

static void add_guest_map(struct acrn_vm *vm, struct hugepage_map *map)
{
	int max_gfn;

	refcount_set(&map->refcount, 1);

	INIT_HLIST_NODE(&map->hlist);

	max_gfn = (map->guest_gpa + map->size) >> PAGE_SHIFT;
	if (vm->max_gfn < max_gfn)
		vm->max_gfn = max_gfn;

	pr_debug("HSM: add hugepage with size=0x%lx,vm0_hpa=0x%llx and its guest gpa = 0x%llx\n",
		 map->size, map->vm0_gpa, map->guest_gpa);

	mutex_lock(&vm->hugepage_lock);
	/* 1G hugepage? */
	if (map->size == (1UL << HUGEPAGE_1G_SHIFT))
		hlist_add_head(&map->hlist,
			       &vm->hugepage_hlist[HUGEPAGE_1G_HLIST_IDX]);
	else
		hlist_add_head(&map->hlist,
			       hlist_2m_hash(vm, map->guest_gpa));
	mutex_unlock(&vm->hugepage_lock);
}

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
