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
 * acrn_mm_gpa2hva - convert guest physical address to host virtual address
 *
 * @vmid: guest vmid
 * @guest_pa: physical address in guest
 * @size: the memory size mapped
 *
 * Return: host kernel virtual address, NULL on error
 */
void *acrn_mm_gpa2hva(unsigned short vmid, u64 guest_pa, size_t size);

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
int acrn_mm_add_region(unsigned short vmid, unsigned long guest_pa,
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
int acrn_mm_del_region(unsigned short vmid, unsigned long guest_pa,
			   unsigned long size);

/**
 * acrn_mm_page_wp - change one page write protection attr
 *
 * @vmid: guest VM vmid
 * @guest_pa: guest physical address
 * @enable_wp: enable/disable write protection of the page on gpa
 *
 * Return: 0 on success, <0 for error.
 */
int acrn_mm_page_wp(unsigned short vmid,
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
int acrn_inject_msi(unsigned short vmid, unsigned long msi_addr,
			   unsigned long msi_data);


/* the API related with emulated mmio ioreq */
typedef	int (*ioreq_handler_t)(int client_id,
			       unsigned long *ioreqs_map,
			       void *client_priv);

/**
 * acrn_ioreq_create_client - create ioreq client
 *
 * @vmid: ID to identify guest
 * @handler: ioreq_handler of ioreq client
 *           If client wants to handle request in client thread context, set
 *           this parameter to NULL. If client wants to handle request out of
 *           client thread context, set handler function pointer of its own.
 *           acrn_hsm will create kernel thread and call handler to handle
 *           request(This is recommended).
 *
 * @client_priv: the private structure for the given client.
 *           When handler is not NULL, this is required and used as the
 *           third argument of ioreq_handler callback
 *
 * @name: the name of ioreq client
 *
 * Return: client id on success, <0 on error
 */
int acrn_ioreq_create_client(unsigned short vmid,
			     ioreq_handler_t handler,
			     void *client_priv,
			     char *name);

/**
 * acrn_ioreq_destroy_client - destroy ioreq client
 *
 * @client_id: client id to identify ioreq client
 *
 * Return:
 */
void acrn_ioreq_destroy_client(int client_id);

/**
 * acrn_ioreq_add_iorange - add iorange monitored by ioreq client
 *
 * @client_id: client id to identify ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_add_iorange(int client_id, uint32_t type,
			   long start, long end);

/**
 * acrn_ioreq_del_iorange - del iorange monitored by ioreq client
 *
 * @client_id: client id to identify ioreq client
 * @type: iorange type
 * @start: iorange start address
 * @end: iorange end address
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_del_iorange(int client_id, uint32_t type,
			   long start, long end);

/**
 * acrn_ioreq_get_reqbuf - get request buffer
 * request buffer is shared by all clients in one guest
 *
 * @client_id: client id to identify ioreq client
 *
 * Return: pointer to request buffer, NULL on error
 */
struct acrn_request *acrn_ioreq_get_reqbuf(int client_id);

/**
 * acrn_ioreq_attach_client - start handle request for ioreq client
 * If request is handled out of client thread context, this function is
 * only called once to be ready to handle new request.
 *
 * If request is handled in client thread context, this function must
 * be called every time after the previous request handling is completed
 * to be ready to handle new request.
 *
 * @client_id: client id to identify ioreq client
 *
 * Return: 0 on success, <0 on error, 1 if ioreq client is destroying
 */
int acrn_ioreq_attach_client(int client_id);

/**
 * acrn_ioreq_complete_request - notify guest request handling is completed
 *
 * @client_id: client id to identify ioreq client
 * @vcpu: identify request submitter
 * @req: the acrn_request that is marked as completed
 *
 * Return: 0 on success, <0 on error
 */
int acrn_ioreq_complete_request(int client_id, uint64_t vcpu,
				struct acrn_request *req);

#endif
