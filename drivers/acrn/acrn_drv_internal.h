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
	 *  true: set the wp; flase: clear the wp
	 */
	u8 set;

	/** Reserved */
	u8 reserved[7];

	/** the guest physical address of the page to change */
	u64 gpa;
};

#define ACRN_INVALID_VMID (-1)

enum ACRN_VM_FLAGS {
	ACRN_VM_DESTROYED = 0,
	ACRN_VM_IOREQ_FREE,
};

extern struct list_head acrn_vm_list;
extern rwlock_t acrn_vm_list_lock;

void vm_list_add(struct list_head *list);

#define HUGEPAGE_2M_HLIST_ARRAY_SIZE	32
#define HUGEPAGE_1G_HLIST_ARRAY_SIZE	1
#define HUGEPAGE_HLIST_ARRAY_SIZE	(HUGEPAGE_2M_HLIST_ARRAY_SIZE + \
					 HUGEPAGE_1G_HLIST_ARRAY_SIZE)
#define ACRN_MAX_REGION_NUM	256
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
 * @hugepage_hlist: hash list of hugepage
 * @ioreq_fallback_client: default ioreq client
 * @ioreq_client_lock: spinlock to protect ioreq_client_list
 * @ioreq_client_list: list of ioreq clients
 * @req_buf: request buffer shared between HV, SOS and UOS
 * @pg: pointer to linux page which holds req_buf
 * @pci_conf_addr: the saved pci_conf1_addr for 0xCF8
 */
struct acrn_vm {
	struct device *dev;
	struct list_head list;
	unsigned short vmid;
	refcount_t refcnt;
	int max_gfn;
	atomic_t vcpu_num;
	unsigned long flags;
	struct page *monitor_page;

	struct mutex regions_mapping_lock;
	struct region_mapping regions_mapping[ACRN_MAX_REGION_NUM];
	int regions_mapping_count;

	int ioreq_fallback_client;
	/* the spin_lock to protect ioreq_client_list */
	spinlock_t ioreq_client_lock;
	struct list_head ioreq_client_list;
	struct acrn_request_buffer *req_buf;
	struct page *pg;
	u32 pci_conf_addr;
};

int acrn_vm_destroy(struct acrn_vm *vm);

struct acrn_vm *find_get_vm(unsigned short vmid);
void get_vm(struct acrn_vm *vm);
void put_vm(struct acrn_vm *vm);
void free_guest_mem(struct acrn_vm *vm);
int map_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int unmap_guest_memseg(struct acrn_vm *vm, struct vm_memmap *memmap);
int hugepage_map_guest(struct acrn_vm *vm, struct vm_memmap *memmap);
void hugepage_free_guest(struct acrn_vm *vm);
void *hugepage_map_guest_phys(struct acrn_vm *vm, u64 guest_phys, size_t size);
int hugepage_unmap_guest_phys(struct acrn_vm *vm, u64 guest_phys);
int set_memory_regions(struct map_regions *regions);

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
int acrn_ioreq_create_fallback_client(unsigned short vmid, char *name);
unsigned int acrn_dev_poll(struct file *filep, poll_table *wait);
void acrn_ioreq_driver_init(void);
void acrn_ioreq_clear_request(struct acrn_vm *vm);
int acrn_ioreq_distribute_request(struct acrn_vm *vm);

/* ioeventfd APIs */
int acrn_ioeventfd_init(unsigned short vmid);
int acrn_ioeventfd_config(unsigned short vmid, struct acrn_ioeventfd *args);
void acrn_ioeventfd_deinit(unsigned short vmid);

/* irqfd APIs */
int acrn_irqfd_init(unsigned short vmid);
int acrn_irqfd_config(unsigned short vmid, struct acrn_irqfd *args);
void acrn_irqfd_deinit(unsigned short vmid);

#endif
