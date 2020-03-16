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
 * @enable_wp: enable/disable write protection of page on gpa
 *
 * Return: 0 on success, <0 for error.
 */
extern int acrn_write_protect_page(unsigned short vmid, unsigned long gpa,
				   bool enable_wp);

/**
 * acrn_inject_msi() - inject MSI interrupt to guest
 *
 * @vmid: guest vmid
 * @msi_addr: MSI addr matches MSI spec
 * @msi_data: MSI data matches MSI spec
 *
 * Return: 0 on success, <0 on error
 */
extern int acrn_inject_msi(unsigned short vmid, unsigned long msi_addr,
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
