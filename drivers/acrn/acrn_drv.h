/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN_HSM : vm management header file.
 *
 */

#ifndef __ACRN_VM_MNGT_H
#define __ACRN_VM_MNGT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/refcount.h>
#include <linux/poll.h>
#include <linux/acrn_host.h>
#include "hypercall.h"

struct acrn_vm;

#define ACRN_MAX_REGION_NUM	256
struct vm_memory_region {
#define MR_ADD		0
#define MR_DEL		2
	u32 type;
	/* IN: memory attribute */
	u32 attr;
	/* IN: physical address of guest VM to be mapped */
	u64 guest_vm_pa;
	/* IN: physical address of host VM to be mapped */
	u64 host_vm_pa;
	/* IN: size of the region */
	u64 size;
};

struct map_regions {
	/* IN: vmid for this hypercall */
	u16 vmid;
	/* Reserved */
	u16 reserved[3];
	/* IN: vm_memory_region number */
	u32 mr_num;
	/* Reserved */
	u32 reserved1;
	/* IN: physical address of vm_memory_region array */
	u64 regions_pa;
};

struct wp_data {
	/** set page write protect permission.
	 *  1: set the wp; 0: clear the wp
	 */
	u8 set;

	/** Reserved */
	u8 reserved[7];

	/** the guest physical address of the page to change */
	u64 gpa;
};

struct ioreq_range {
	struct list_head list;
	uint32_t type;
	long start;
	long end;
};

struct region_mapping {
	struct page **pages;
	int npages;
	void *host_vm_va;
	u64 guest_vm_pa;
	size_t size;
};

enum IOREQ_CLIENT_BITS {
	IOREQ_CLIENT_DESTROYING,
};

#define ACRN_INVALID_VMID (-1)
enum ACRN_VM_FLAGS {
	ACRN_VM_DESTROYED
};

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;

struct acrn_vm {
	struct list_head list;
	uint16_t vmid;
	/* maximum VM page frame number */
	int max_gfn;
	/* VM vcpu number */
	int vcpu_num;
	unsigned long flags;
	struct page *monitor_page;

	struct mutex regions_mapping_lock;
	/* regions of this VM */
	struct region_mapping regions_mapping[ACRN_MAX_REGION_NUM];
	int regions_mapping_count;

	/* the default ioreq client of this VM */
	int ioreq_default_client;
	spinlock_t ioreq_clients_lock;
	/* ioreq clients list in this VM */
	struct list_head ioreq_clients;
	/* IO request shared buffer between host kernel and hypervisor */
	struct acrn_request_buffer *req_buf;
	/* the page hold the IO request shared buffer */
	struct page *ioreq_page;

	/* ioeventfd */
	struct mutex ioeventfds_lock;
	struct list_head ioeventfds;
	struct ioreq_client *ioeventfd_client;

	/* irqfd */
	struct mutex irqfds_lock;
	struct list_head irqfds;
	struct workqueue_struct *irqfd_wq;

	/* cache address of pci configuration R/W through PIO */
	u32 pci_conf_addr;
};


struct acrn_vm *acrn_vm_create(struct acrn_vm *vm,
		struct acrn_create_vm *vm_param);
int acrn_vm_destroy(struct acrn_vm *vm);

int map_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int unmap_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int map_guest_ram(struct acrn_vm *vm, struct vm_memmap *memmap);
void unmap_guest_all_ram(struct acrn_vm *vm);

/**
 * @brief Info to set ioreq buffer for a created VM
 *
 * the parameter for HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	/** host physical address of VM request_buffer */
	u64 req_buf;
};

/* ioreq */
int acrn_ioreq_init(struct acrn_vm *vm, unsigned long buf_vma);
void acrn_ioreq_deinit(struct acrn_vm *vm);
void acrn_setup_ioreq_intr(void);
void acrn_ioreq_clear_request(struct acrn_vm *vm);
int acrn_ioreq_attach_client(struct ioreq_client *client);
int acrn_ioreq_complete_request(struct ioreq_client *client, unsigned long vcpu,
				struct acrn_request *acrn_req);
struct ioreq_client *get_default_client(struct acrn_vm *vm);

/* ioeventfd APIs */
int acrn_ioeventfd_init(struct acrn_vm *vm);
int acrn_ioeventfd_config(struct acrn_vm *vm, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(struct acrn_vm *vm);

/* irqfd APIs */
int acrn_irqfd_init(struct acrn_vm *vm);
int acrn_irqfd_config(struct acrn_vm *vm, struct acrn_irqfd *args);
void acrn_irqfd_deinit(struct acrn_vm *vm);

#endif
