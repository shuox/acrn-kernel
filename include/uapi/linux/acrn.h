/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause */
/*
 * Userspace interface for /dev/acrn_hsm - ACRN Hypervisor Service Module
 *
 * This file can be used by applications that need to communicate with the HSM
 * via the ioctl interface.
 */

#ifndef _UAPI_ACRN_H
#define _UAPI_ACRN_H

#include <linux/types.h>

#define ACRN_IO_REQUEST_MAX		16

#define ACRN_IOREQ_STATE_PENDING	0
#define ACRN_IOREQ_STATE_COMPLETE	1
#define ACRN_IOREQ_STATE_PROCESSING	2
#define ACRN_IOREQ_STATE_FREE		3

#define ACRN_IOREQ_TYPE_PORTIO		0
#define ACRN_IOREQ_TYPE_MMIO		1
#define ACRN_IOREQ_TYPE_PCICFG		2

#define ACRN_IOREQ_DIR_READ		0
#define ACRN_IOREQ_DIR_WRITE		1

struct acrn_mmio_request {
	__u32 direction;
	__u32 reserved;
	__u64 address;
	__u64 size;
	__u64 value;
} __attribute__((aligned(8)));

struct acrn_pio_request {
	__u32 direction;
	__u32 reserved;
	__u64 address;
	__u64 size;
	__u32 value;
} __attribute__((aligned(8)));

struct acrn_pci_request {
	__u32 direction;
	__u32 reserved[3];/* need keep same header fields with pio_request */
	__u64 size;
	__u32 value;
	__u32 bus;
	__u32 dev;
	__u32 func;
	__u32 reg;
} __attribute__((aligned(8)));

/**
 * struct acrn_io_request - 256-byte ACRN I/O request
 *
 * The state transitions of ACRN I/O request:
 *
 *    FREE -> PENDING -> PROCESSING -> COMPLETE -> FREE -> ...
 *                                \              /
 *                                 +--> FAILED -+
 *
 * When a request is in the COMPLETE or FREE state, the request is owned by the
 * hypervisor. When a request is in the PENDING or PROCESSING state, the
 * request is owned by the Service VM.
 *
 * On basis of the states illustrated above, a typical lifecycle of ACRN IO
 * request would look like:
 *
 * Flow                 (assume the initial state is FREE)
 * |
 * |   Service VM vCPU 0     Service VM vCPU x      User vCPU y
 * |
 * |                                             hypervisor:
 * |                                               fills in type, addr, etc.
 * |                                               pauses the User VM vCPU y
 * |                                               sets the state to PENDING (a)
 * |                                               fires an upcall to Service VM
 * |
 * | HSM:
 * |  scans for PENDING requests
 * |  sets the states to PROCESSING (b)
 * |  assigns the requests to clients (c)
 * V
 * |                     client:
 * |                       scans for the assigned requests
 * |                       handles the requests (d)
 * |                     HSM:
 * |                       sets states to COMPLETE
 * |                       notifies the hypervisor
 * |
 * |                     hypervisor:
 * |                       resumes User VM vCPU y (e)
 * |
 * |                                             hypervisor:
 * |                                               post handling (f)
 * V                                               sets states to FREE
 *
 * Note that the procedures (a) to (f) in the illustration above require to be
 * strictly processed in the order.  One vCPU cannot trigger another request of
 * I/O emulation before completing the previous one.
 *
 * Atomic and barriers are required when HSM and hypervisor accessing the state
 * of acrn_io_request.
 *
 */
struct acrn_io_request {
	/**
	 * @type: Type of this request. Byte offset: 0.
	 */
	__u32 type;

	/**
	 * @completion_polling: Polling flag. Byte offset: 4.
	 *
	 * Hypervisor will poll completion if this flag set.
	 */
	__u32 completion_polling;

	/**
	 * @reserved0: Reserved fields. Byte offset: 8.
	 */
	__u32 reserved0[14];

	/**
	 * @reqs: Union of different types of request. Byte offset: 64.
	 */
	union {
		struct acrn_pio_request pio_request;
		struct acrn_pci_request pci_request;
		struct acrn_mmio_request mmio_request;
		__u64 reserved1[8];
	} reqs;

	/**
	 * @reserved1: Reserved fields. Byte offset: 128.
	 */
	__u32 reserved1;

	/**
	 * @kernel_handled: Kernel handled flag. Byte offset: 132.
	 *
	 * If this request is handled in kernel.
	 */
	__u32 kernel_handled;

	/**
	 * @processed: The status of this request. Byte offset: 136.
	 *
	 * Take ACRN_IOREQ_STATE_xxx as values.
	 */
	__u32 processed;
} __attribute__((aligned(256)));

struct acrn_io_request_buffer {
	union {
		struct acrn_io_request req_slot[ACRN_IO_REQUEST_MAX];
		__u8 reserved[4096];
	};
};

/**
 * struct acrn_ioreq_notify - The structure of ioreq completion notification
 *
 * @vmid: VM ID
 * @vcpu: vCPU ID
 */
struct acrn_ioreq_notify {
	__u32 vmid;
	__u32 vcpu;
} __attribute__((aligned(8)));

/**
 * struct acrn_api_version - Structure of ACRN API version.
 * @major_version: Major version of ACRN API.
 * @minor_version: Minor version of ACRN API.
 */
struct acrn_api_version {
	__u32 major_version;
	__u32 minor_version;
} __attribute__((aligned(8)));

/**
 * struct acrn_create_vm - Info to create a VM
 * @vmid: VM ID returned from the hypervisor
 * @reserved0: Reserved
 * @vcpu_num: vCPU number of VM
 * @reserved1: Reserved
 * @uuid: UUID of the VM
 * @vm_flag: Flag of the VM creating
 * @req_buf: Service VM GPA of I/O request buffer
 * @cpu_affinity: CPU affinity of the VM
 * @reserved2: Reserved
 */
struct acrn_create_vm {
	__u16	vmid;
	__u16	reserved0;
	__u16	vcpu_num;
	__u16	reserved1;
	__u8	uuid[16];
	__u64	vm_flag;
	__u64	req_buf;
	__u64	cpu_affinity;
	__u8	reserved2[8];
} __attribute__((aligned(8)));

struct acrn_gp_regs {
	__u64 rax;
	__u64 rcx;
	__u64 rdx;
	__u64 rbx;
	__u64 rsp;
	__u64 rbp;
	__u64 rsi;
	__u64 rdi;
	__u64 r8;
	__u64 r9;
	__u64 r10;
	__u64 r11;
	__u64 r12;
	__u64 r13;
	__u64 r14;
	__u64 r15;
};

struct acrn_descriptor_ptr {
	__u16 limit;
	__u64 base;
	__u16 reserved[3];
} __attribute__ ((__packed__));

struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;
	struct acrn_descriptor_ptr gdt;
	struct acrn_descriptor_ptr idt;

	__u64 rip;
	__u64 cs_base;
	__u64 cr0;
	__u64 cr4;
	__u64 cr3;
	__u64 ia32_efer;
	__u64 rflags;
	__u64 reserved_64[4];

	__u32 cs_ar;
	__u32 cs_limit;
	__u32 reserved_32[3];

	__u16 cs_sel;
	__u16 ss_sel;
	__u16 ds_sel;
	__u16 es_sel;
	__u16 fs_sel;
	__u16 gs_sel;
	__u16 ldt_sel;
	__u16 tr_sel;

	__u16 reserved_16[4];
};

/**
 * struct acrn_set_vcpu_regs - Info of vCPU registers state setting
 * @vcpu_id: vCPU ID
 * @reserved0: Reserved
 * @vcpu_regs: vCPU registers state
 */
struct acrn_set_vcpu_regs {
	__u16 vcpu_id;
	__u16 reserved0[3];
	struct acrn_vcpu_regs vcpu_regs;
} __attribute__((aligned(8)));

#define	ACRN_MEM_ACCESS_RIGHT_MASK	0x00000007U
#define	ACRN_MEM_ACCESS_READ		0x00000001U
#define	ACRN_MEM_ACCESS_WRITE		0x00000002U
#define	ACRN_MEM_ACCESS_EXEC		0x00000004U
#define	ACRN_MEM_ACCESS_RWX		(ACRN_MEM_ACCESS_READ  | \
					 ACRN_MEM_ACCESS_WRITE | \
					 ACRN_MEM_ACCESS_EXEC)

#define	ACRN_MEM_TYPE_MASK		0x000007C0U
#define	ACRN_MEM_TYPE_WB		0x00000040U
#define	ACRN_MEM_TYPE_WT		0x00000080U
#define	ACRN_MEM_TYPE_UC		0x00000100U
#define	ACRN_MEM_TYPE_WC		0x00000200U
#define	ACRN_MEM_TYPE_WP		0x00000400U

/* Memory mapping types */
#define	ACRN_MEMMAP_RAM			0
#define	ACRN_MEMMAP_MMIO		1

/**
 * struct acrn_vm_memmap - A EPT memory mapping info for a User VM.
 * @type: Type of the memory mapping.
 * @reserved: Reserved.
 * @user_vm_pa: Physical address of User VM.
 * @service_vm_pa: Physical address of Service VM.
 * @vma_base: VMA address of Service VM.
 * @len: Length of the memory mapping.
 * @attr: Attribute of the memory mapping.
 */
struct acrn_vm_memmap {
	__u32 type;
	__u32 reserved;
	__u64 user_vm_pa;
	union {
		__u64 service_vm_pa;
		__u64 vma_base;
	};
	__u64 len;
	__u32 attr;
} __attribute__((aligned(8)));

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/*
 * Common IOCTL IDs definition for ACRN userspace
 */
#define ACRN_IOCTL_GET_API_VERSION	\
	_IOR(ACRN_IOCTL_TYPE, 0, struct acrn_api_version)

#define ACRN_IOCTL_CREATE_VM		\
	_IOWR(ACRN_IOCTL_TYPE, 0x10, struct acrn_create_vm)
#define ACRN_IOCTL_DESTROY_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x11)
#define ACRN_IOCTL_START_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x12)
#define ACRN_IOCTL_PAUSE_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x13)
#define ACRN_IOCTL_RESET_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x15)
#define ACRN_IOCTL_SET_VCPU_REGS	\
	_IOW(ACRN_IOCTL_TYPE, 0x16, struct acrn_set_vcpu_regs)

#define ACRN_IOCTL_NOTIFY_REQUEST_FINISH \
	_IOW(ACRN_IOCTL_TYPE, 0x31, struct acrn_ioreq_notify)
#define ACRN_IOCTL_CREATE_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x32)
#define ACRN_IOCTL_ATTACH_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x33)
#define ACRN_IOCTL_DESTROY_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x34)
#define ACRN_IOCTL_CLEAR_VM_IOREQ	\
	_IO(ACRN_IOCTL_TYPE, 0x35)

#define ACRN_IOCTL_SET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x41, struct acrn_vm_memmap)
#define ACRN_IOCTL_UNSET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x42, struct acrn_vm_memmap)

#endif /* _UAPI_ACRN_H */
