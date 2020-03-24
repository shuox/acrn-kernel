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

struct region_mapping {
	struct page **pages;
	int npages;
	void *host_vm_va;
	u64 guest_vm_pa;
	size_t size;
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

enum IOREQ_CLIENT_BITS {
	IOREQ_CLIENT_DESTROYING = 0,
	IOREQ_CLIENT_EXIT,
};

struct ioreq_client {
	/* client name */
	char name[16];
	/* client id */
	int id;
	/* vm this client belongs to */
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

#define ACRN_INVALID_VMID (-1)
enum ACRN_VM_FLAGS {
	ACRN_VM_DESTROYED = 0,
};
extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;
/**
 * struct acrn_vm - data structure to track guest
 *
 * @dev: pointer to dev of linux device mode
 * @list: list of acrn_vm
 * @vmid: guest vmid
 * @refcnt: reference count of guest
 * @max_gfn: maximum guest page frame number
 * @vcpu_num: vcpu number
 * @flags: VM flag bits
 * @monitor_page: the page for monitor interrupt
 * @ioreq_default_client: default ioreq client
 * @ioreq_client_lock: spinlock to protect ioreq_client_list
 * @ioreq_client_list: list of ioreq clients
 * @req_buf: request buffer shared between HV, SOS and UOS
 * @ioreq_page: pointer to linux page which holds req_buf
 * @pci_conf_addr: the saved pci_conf1_addr for 0xCF8
 */
struct acrn_vm {
	struct device *dev;
	struct list_head list;
	uint16_t vmid;
	refcount_t refcnt;
	int max_gfn;
	atomic_t vcpu_num;
	unsigned long flags;
	struct page *monitor_page;

	struct mutex regions_mapping_lock;
	struct region_mapping regions_mapping[ACRN_MAX_REGION_NUM];
	int regions_mapping_count;

	int ioreq_default_client;
	spinlock_t ioreq_clients_lock;
	struct list_head ioreq_clients;
	struct acrn_request_buffer *req_buf;
	struct page *ioreq_page;

	u32 pci_conf_addr;
};

struct acrn_vm *acrn_vm_id_get(uint16_t vmid);
void acrn_vm_get(struct acrn_vm *vm);
void acrn_vm_put(struct acrn_vm *vm);
void acrn_vm_register(struct acrn_vm *vm);
void acrn_vm_deregister(struct acrn_vm *vm);
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

int acrn_ioreq_init(struct acrn_vm *vm, unsigned long vma);
void acrn_ioreq_free(struct acrn_vm *vm);
int acrn_ioreq_create_default_client(uint16_t vmid, char *name);
void acrn_ioreq_driver_init(void);
void acrn_ioreq_clear_request(struct acrn_vm *vm);

/* ioeventfd APIs */
int acrn_ioeventfd_init(uint16_t vmid);
int acrn_ioeventfd_config(uint16_t vmid, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(uint16_t vmid);

/* irqfd APIs */
int acrn_irqfd_init(uint16_t vmid);
int acrn_irqfd_config(uint16_t vmid, struct acrn_irqfd *args);
void acrn_irqfd_deinit(uint16_t vmid);

#endif
