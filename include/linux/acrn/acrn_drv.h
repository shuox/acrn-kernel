/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_drv.h
 *
 * ACRN HSM exported API for other modules.
 */

#ifndef _ACRN_DRV_H
#define _ACRN_DRV_H

#include <linux/types.h>
#include <linux/acrn/acrn_common_def.h>

/**
 * acrn_map_guest_phys - map guest physical address to SOS kernel
 *			 virtual address
 *
 * @vmid: guest vmid
 * @uos_phys: physical address in guest
 * @size: the memory size mapped
 *
 * Return: SOS kernel virtual address, NULL on error
 */
extern void *acrn_map_guest_phys(unsigned short vmid, u64 uos_phys,
				 size_t size);

/**
 * acrn_unmap_guest_phys - unmap guest physical address
 *
 * @vmid: guest vmid
 * @uos_phys: physical address in guest
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_unmap_guest_phys(unsigned short vmid, u64 uos_phys);

/**
 * acrn_add_memory_region - add a guest memory region
 *
 * @vmid: guest vmid
 * @gpa: gpa of UOS
 * @host_gpa: gpa of SOS
 * @size: memory region size
 * @mem_type: memory mapping type. Possible value could be:
 *                    MEM_TYPE_WB
 *                    MEM_TYPE_WT
 *                    MEM_TYPE_UC
 *                    MEM_TYPE_WC
 *                    MEM_TYPE_WP
 * @mem_access_right: memory mapping access. Possible value could be:
 *                    MEM_ACCESS_READ
 *                    MEM_ACCESS_WRITE
 *                    MEM_ACCESS_EXEC
 *                    MEM_ACCESS_RWX
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_add_memory_region(unsigned short vmid, unsigned long gpa,
				  unsigned long host_gpa, unsigned long size,
				  unsigned int mem_type,
				  unsigned int mem_access_right);

/**
 * acrn_del_memory_region - delete a guest memory region
 *
 * @vmid: guest vmid
 * @gpa: gpa of UOS
 * @size: memory region size
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_del_memory_region(unsigned short vmid, unsigned long gpa,
			   unsigned long size);

/**
 * write_protect_page - change one page write protection
 *
 * @vmid: guest vmid
 * @gpa: gpa in guest vmid
 * @set: set or clear page write protection
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_write_protect_page(unsigned short vmid, unsigned long gpa,
				   unsigned char set);

#endif
