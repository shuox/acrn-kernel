/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/acrn.h>
#include <linux/types.h>

#include "hypercall.h"

#define ACRN_NAME_LEN		16
#define ACRN_MEM_MAPPING_MAX	256

#define ACRN_MEM_REGION_ADD	0
#define ACRN_MEM_REGION_DEL	2

struct acrn_vm;
struct acrn_ioreq_client;

/**
 * struct vm_memory_region - Basic VM memory region setup info.
 * @type: Operation type of this memory region.
 * @attr: Memory attribute.
 * @user_vm_pa: Physical address of User VM to be mapped.
 * @service_vm_pa: Physical address of Service VM to be mapped.
 * @size: Size of this region.
 *
 * The basic info for creating EPT mapping of User VM.
 */
struct vm_memory_region {
	u32 type;
	u32 attr;
	u64 user_vm_pa;
	u64 service_vm_pa;
	u64 size;
};

/**
 * struct vm_memory_region_list - A batch of vm_memory_region.
 * @vmid: The target VM ID.
 * @reserved: Reserved.
 * @regions_num: The number of vm_memory_region.
 * @reserved1: Reserved.
 * @regions_gpa: Physical address of a vm_memory_region array.
 *
 * Hypercall HC_VM_SET_MEMORY_REGIONS uses this structure to set up multiple
 * memory regions for a User VM. A vm_memory_region_list contains multiple
 * vm_memory_region for batch processing in the ACRN hypervisor.
 */
struct vm_memory_region_list {
	u16 vmid;
	u16 reserved[3];
	u32 regions_num;
	u32 reserved1;
	u64 regions_gpa;
};

/**
 * struct vm_memory_mapping - Memory map between a User VM and the Service VM
 * @pages: Pages in Service VM kernel.
 * @npages: Number of pages.
 * @service_vm_va: Virtual address in Service VM kernel.
 * @user_vm_pa: Physical address in User VM.
 * @size: Size of this memory region.
 *
 * HSM maintains memory mappings between a User VM GPA and the Service VM
 * kernel VA for kernel based device model emulation.
 */
struct vm_memory_mapping {
	struct page **pages;
	int npages;
	void *service_vm_va;
	u64 user_vm_pa;
	size_t size;
};

/**
 * struct acrn_set_ioreq_buffer - Data for setting the ioreq buffer of User VM
 * @req_buf: The GPA of the IO request shared buffer of a VM
 *
 * The parameter for the HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	u64 req_buf;
};

struct acrn_ioreq_range {
	struct list_head list;
	u32 type;
	u64 start;
	u64 end;
};

#define ACRN_IOREQ_CLIENT_DESTROYING	0U
typedef	int (*ioreq_handler_t)(struct acrn_ioreq_client *client,
			       struct acrn_io_request *req);
/**
 * struct acrn_ioreq_client - Structure of I/O client.
 * @name: Client name
 * @vm: The VM that the client belongs to
 * @list: List node for this acrn_ioreq_client
 * @is_default: If this client is the default one
 * @flags: Flags
 * @range_list: I/O ranges
 * @range_lock: Lock to protect range_list
 * @ioreqs_map: The pending I/O requests bitmap
 * @handler: I/O requests handler of this client
 * @thread: The thread which executes the handler
 * @wq: The wait queue for the handler thread parking
 * @priv: Data for the thread
 */
struct acrn_ioreq_client {
	char			name[ACRN_NAME_LEN];
	struct acrn_vm		*vm;
	struct list_head	list;
	bool			is_default;
	unsigned long		flags;
	struct list_head	range_list;
	rwlock_t		range_lock;

	DECLARE_BITMAP(ioreqs_map, ACRN_IO_REQUEST_MAX);
	ioreq_handler_t		handler;
	struct task_struct	*thread;
	wait_queue_head_t	wq;
	void			*priv;
};

#define ACRN_INVALID_VMID (0xffffU)

#define ACRN_VM_FLAG_DESTROYED		0U
#define ACRN_VM_FLAG_CLEARING_IOREQ	1U
extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
/**
 * struct acrn_vm - Structure of ACRN VM.
 * @list: list node to link all VMs
 * @vmid: VM ID
 * @vcpu_num: vCPU number of the VM
 * @flags: flags of the VM
 * @regions_mapping_lock: Lock to protect regions_mapping.
 * @regions_mapping: Memory mappings of this VM.
 * @regions_mapping_count: Exist number of memory mapping of this VM.
 * @ioreq_clients_lock: Lock to protect ioreq_clients and default_client
 * @ioreq_clients: The I/O request clients list of this VM
 * @default_client: The default I/O request client
 * @req_buf: I/O request shared buffer
 * @ioreq_page: The page of the I/O request shared buffer
 * @pci_conf_addr: Address of a PCI configuration access emulation
 * @ioeventfds_lock: Lock to protect ioeventfds list
 * @ioeventfds: List to link all hsm_ioeventfd
 * @ioeventfd_client: I/O client for all ioeventfd belong to the VM
 * @irqfds_lock: Lock to protect irqfds list
 * @irqfds: List to link all hsm_irqfd
 * @irqfd_wq: workqueue for irqfd async shutdown
 */
struct acrn_vm {
	struct list_head	list;
	u16			vmid;
	int			vcpu_num;
	unsigned long		flags;
	struct page		*monitor_page;

	struct mutex		 regions_mapping_lock;
	struct vm_memory_mapping regions_mapping[ACRN_MEM_MAPPING_MAX];
	int 			 regions_mapping_count;

	spinlock_t		 ioreq_clients_lock;
	struct list_head	 ioreq_clients;
	struct acrn_ioreq_client *default_client;
	struct acrn_io_request_buffer *req_buf;
	struct page		 *ioreq_page;
	u32			 pci_conf_addr;

	struct mutex			ioeventfds_lock;
	struct list_head		ioeventfds;
	struct acrn_ioreq_client	*ioeventfd_client;

	struct mutex			irqfds_lock;
	struct list_head		irqfds;
	struct workqueue_struct		*irqfd_wq;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
			       struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa,
		       u64 size, u32 mem_type, u32 mem_access_right);
int acrn_mm_del_region(u16 vmid, u64 user_gpa, u64 size);
int acrn_map_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_unmap_vm_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_map_vm_ram(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
void acrn_unmap_vm_all_ram(struct acrn_vm *vm);

int acrn_ioreq_init(struct acrn_vm *vm, u64 buf_vma);
void acrn_ioreq_deinit(struct acrn_vm *vm);
void acrn_setup_ioreq_intr(void);
void acrn_remove_ioreq_intr(void);
void acrn_ioreq_clear_request(struct acrn_vm *vm);
int acrn_ioreq_wait_client(struct acrn_ioreq_client *client);
int acrn_ioreq_complete_request_default(struct acrn_vm *vm, u16 vcpu);
struct acrn_ioreq_client *acrn_ioreq_create_client(struct acrn_vm *vm,
						   ioreq_handler_t handler,
						   void *data, bool is_default,
						   const char *name);
void acrn_ioreq_destroy_client(struct acrn_ioreq_client *client);
int acrn_ioreq_add_range(struct acrn_ioreq_client *client,
			 u32 type, u64 start, u64 end);
int acrn_ioreq_del_range(struct acrn_ioreq_client *client,
			 u32 type, u64 start, u64 end);

int acrn_inject_msi(u16 vmid, u64 msi_addr, u64 msi_data);

int acrn_ioeventfd_init(struct acrn_vm *vm);
int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(struct acrn_vm *vm);

int acrn_irqfd_init(struct acrn_vm *vm);
int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args);
void acrn_irqfd_deinit(struct acrn_vm *vm);

#endif /* __ACRN_HSM_DRV_H */
