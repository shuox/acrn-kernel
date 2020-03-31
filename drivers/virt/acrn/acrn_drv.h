/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/acrn.h>
#include <linux/acrn_hsm.h>

#include "hypercall.h"

#define ACRN_MEM_MAPPING_MAX	256

/**
 * struct vm_memory_region - Basic VM memory region setup info
 *
 * The basic info for creating EPT mapping of User VM.
 */
struct vm_memory_region {
#define ACRN_MEM_REGION_ADD	0
#define ACRN_MEM_REGION_DEL	2
	/** Operation type of this memory region */
	u32 type;
	/** Memory attribute */
	u32 attr;
	/** Physical address of User VM to be mapped */
	u64 user_vm_pa;
	/** Physical address of Service VM to be mapped */
	u64 service_vm_pa;
	/** Size of this region */
	u64 size;
};

/**
 * struct vm_memory_region_list - A batch of vm_memory_region
 *
 * Hypercall HC_VM_SET_MEMORY_REGIONS uses this structure to set up multiple
 * memory regions for a User VM.  A vm_memory_region_list contains multiple
 * vm_memory_region for batch processing in the ACRN hypervisor.
 */
struct vm_memory_region_list {
	/** The target VM's ID */
	u16 vmid;
	/** Reserved */
	u16 reserved[3];
	/** The number of vm_memory_region */
	u32 regions_num;
	/** Reserved */
	u32 reserved1;
	/** Physical address of a vm_memory_region array */
	u64 regions_gpa;
};

/*
 * struct vm_memory_mapping - Memory map between a User VM and the Service VM
 *
 * HSM maintains memory mappings between a User VM GPA and the Service VM
 * kernel VA for kernel based device model emulation.
 */
struct vm_memory_mapping {
	/** pages in Service VM kernel */
	struct page **pages;
	/** Number of pages */
	int npages;
	/** Virtual address in Service VM kernel */
	void *service_vm_va;
	/** Physical address in User VM */
	u64 user_vm_pa;
	/** Size of this memory region */
	size_t size;
};

/**
 * The data for setting the ioreq buffer for a User VM
 *
 * The parameter for the HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	/** The GPA of the IO request shared buffer of a VM */
	u64 req_buf;
};

struct acrn_ioreq_range {
	struct list_head list;
	u32 type;
	u64 start;
	u64 end;
};

#define ACRN_INVALID_VMID (0xffffU)

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
struct acrn_vm {
	/* list node to link all VMs */
	struct list_head list;
	/* VM ID */
	u16	vmid;
	/* vCPU number of the VM */
	int	vcpu_num;

#define ACRN_VM_FLAG_DESTROYED		0U
#define ACRN_VM_FLAG_CLEARING_IOREQ	1U
	unsigned long flags;
	struct page *monitor_page;

	/** Lock to protect regions_mapping */
	struct mutex regions_mapping_lock;
	/** Memory mappings of this VM */
	struct vm_memory_mapping regions_mapping[ACRN_MEM_MAPPING_MAX];
	/** Exist number of memory mapping of this VM */
	int regions_mapping_count;

	spinlock_t ioreq_clients_lock;
	/* The ioreq clients list of this VM */
	struct list_head ioreq_clients;
	/* The default ioreq client */
	struct acrn_ioreq_client *default_client;
	/* IO request shared buffer */
	struct acrn_io_request_buffer *req_buf;
	/* The page hold the IO request shared buffer */
	struct page *ioreq_page;
	/* Address of a PCI configuration access emulation */
	u32 pci_conf_addr;

	/* ioeventfd */
	struct mutex ioeventfds_lock;
	struct list_head ioeventfds;
	struct acrn_ioreq_client *ioeventfd_client;

	/* irqfd */
	struct mutex irqfds_lock;
	struct list_head irqfds;
	struct workqueue_struct *irqfd_wq;
};

struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
		struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);

int acrn_map_guest_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_unmap_guest_memseg(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
int acrn_map_guest_ram(struct acrn_vm *vm, struct acrn_vm_memmap *memmap);
void acrn_unmap_guest_all_ram(struct acrn_vm *vm);

int acrn_ioreq_init(struct acrn_vm *vm, u64 buf_vma);
void acrn_ioreq_deinit(struct acrn_vm *vm);
void acrn_setup_ioreq_intr(void);
void acrn_ioreq_clear_request(struct acrn_vm *vm);
int acrn_ioreq_wait_client(struct acrn_ioreq_client *client);
int acrn_ioreq_complete_request_default(struct acrn_vm *vm, u16 vcpu);

int acrn_ioeventfd_init(struct acrn_vm *vm);
int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(struct acrn_vm *vm);

int acrn_irqfd_init(struct acrn_vm *vm);
int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args);
void acrn_irqfd_deinit(struct acrn_vm *vm);

#endif
