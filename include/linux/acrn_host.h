/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_drv.h
 *
 * ACRN HSM exported API for other modules in host.
 */

#ifndef _ACRN_HOST_H
#define _ACRN_HOST_H

#include <linux/types.h>
#include <linux/acrn.h>

struct acrn_vm;
struct ioreq_client;

/**
 * acrn_mm_gpa2hva - convert guest physical address to host virtual address
 *
 * @vm: guest VM
 * @guest_pa: physical address in guest
 * @size: the memory size mapped
 *
 * Return: host kernel virtual address, NULL on error
 */
extern void *acrn_mm_gpa2hva(struct acrn_vm *vm, u64 guest_pa, size_t size);

/**
 * acrn_mm_add_region - add a guest memory region
 *
 * @vmid: guest VM vmid
 * @guest_pa: guest VM physical address
 * @host_pa: host VM physical address
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
extern int acrn_mm_add_region(unsigned short vmid, unsigned long guest_pa,
			   unsigned long host_pa, unsigned long size,
			   unsigned int mem_type,
			   unsigned int mem_access_right);

/**
 * acrn_mm_del_region - delete a guest memory region
 *
 * @vmid: guest VM vmid
 * @guest_pa: guest physical address
 * @size: memory region size
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_mm_del_region(unsigned short vmid,
		unsigned long guest_pa, unsigned long size);

/**
 * acrn_mm_page_wp - change one page write protection attr
 *
 * @vmid: guest VM vmid
 * @guest_pa: guest physical address
 * @enable_wp: enable/disable write protection of the page on gpa
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_mm_page_wp(unsigned short vmid,
		unsigned long guest_pa, bool enable_wp);

/**
 * acrn_inject_msi() - inject MSI interrupt to guest
 *
 * @vmid: guest vmid
 * @msi_addr: MSI addr matches MSI spec
 * @msi_data: MSI data matches MSI spec
 *
 * Return: 0 on success, <0 on error
 */
int acrn_inject_msi(unsigned short vmid,
		unsigned long msi_addr, unsigned long msi_data);


/* the API related with emulated mmio ioreq */
typedef	int (*ioreq_handler_t)(struct ioreq_client *client,
				struct acrn_request *req);

struct ioreq_client {
	/* client name */
	char name[16];
	/* VM this client belongs to */
	struct acrn_vm *vm;
	/* list node for this ioreq_client */
	struct list_head list;
	/*
	 * This flag indicates if this is the default client
	 * Each VM has a default client.
	 */
	bool is_default;

	unsigned long flags;

	/* client covered io ranges - N/A for default client */
	struct list_head range_list;
	rwlock_t range_lock;

	/* records the pending IO requests of corresponding vcpu */
	DECLARE_BITMAP(ioreqs_map, ACRN_REQUEST_MAX);

	/* IO requests handler of this client */
	ioreq_handler_t handler;
	struct task_struct *thread;
	wait_queue_head_t wq;
	void *priv;
};

/**
 * acrn_ioreq_create_client - create ioreq client
 *
 * @vm: the VM this client belongs to
 * @handler: ioreq_handler of ioreq client
 *           acrn_hsm will create kernel thread and call handler to handle
 *           request. An exception is the default client doesn't have handler
 *           in kernel, it has a userspace thread instead.
 * @data: passed-in data structure for handler
 * @is_default: if it is the default client
 * @name: the name of ioreq client
 *
 * Return: client id on success, <0 on error
 */
struct ioreq_client *acrn_ioreq_create_client(struct acrn_vm *vm,
				ioreq_handler_t handler, void *data,
				bool is_default, char *name);

/**
 * acrn_ioreq_destroy_client - destroy ioreq client
 *
 * @client: ioreq client
 *
 * Return:
 */
void acrn_ioreq_destroy_client(struct ioreq_client *client);

/**
 * acrn_ioreq_add_range - add iorange monitored by ioreq client
 *
 * @client: ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_add_range(struct ioreq_client *client, uint32_t type,
			   long start, long end);

/**
 * acrn_ioreq_del_range - del iorange monitored by ioreq client
 *
 * @client: ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_del_range(struct ioreq_client *client, uint32_t type,
			   long start, long end);

#endif
