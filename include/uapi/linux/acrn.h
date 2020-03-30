/* SPDX-License-Identifier: (GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause */
/**
 * @file acrn.h
 *
 * Userspace interface for /dev/acrn_hsm
 * Common structure/definitions for acrn ioctl
 */

#ifndef _ACRN_H
#define _ACRN_H

#include <linux/types.h>

/**
 * struct acrn_api_version - structure of ACRN API version
 *
 * @major_version: major version of ACRN API
 * @minor_version: minor version of ACRN API
 */
struct acrn_api_version {
	__u32 major_version;
	__u32 minor_version;
} __aligned(8);

/**
 * @brief Info to create a VM
 */
struct acrn_create_vm {
	/** VM ID returned from the hypervisor */
	__u16 vmid;
	/** Reserved */
	__u16 reserved0;
	/** vCPU number of VM */
	__u16 vcpu_num;
	/** Reserved */
	__u16 reserved1;
	/** the uuid of VM */
	__u8 uuid[16];
	/* VM flag bits of VM */
	__u64 vm_flag;
	/** Service VM GPA of I/O request buffer */
	__u64 req_buf;
	/** the CPU affinity of VM */
	__u64 cpu_affinity;
	/** Reserved */
	__u8 reserved2[8];
} __aligned(8);

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
} __packed;

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
 * Info of vCPU registers setting
 */
struct acrn_set_vcpu_regs {
	/** vCPU ID */
	__u16 vcpu_id;
	/** reserved */
	__u16 reserved0[3];
	/** vCPU registers */
	struct acrn_vcpu_regs vcpu_regs;
} __aligned(8);

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
 * struct acrn_vm_memmap - A EPT memory mapping info for a User VM
 */
struct acrn_vm_memmap {
	/** Type of the memory mapping */
	__u32 type;
	__u32 reserved;
	/** Physical address of User VM */
	__u64 user_vm_pa;
	union {
		/** Physical address of Service VM */
		__u64 service_vm_pa;
		/** VMA address of Service VM */
		__u64 vma_base;
	};
	/** Length of the memory mapping */
	__u64 len;
	/** Attribute of the memory mapping */
	__u32 attr;
} __aligned(8);

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/**
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

#define ACRN_IOCTL_SET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x41, struct acrn_vm_memmap)
#define ACRN_IOCTL_UNSET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x42, struct acrn_vm_memmap)

#endif /* _ACRN_H */
