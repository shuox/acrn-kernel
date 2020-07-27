/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Userspace interface for /dev/acrn_hsm - ACRN Hypervisor Service Module
 *
 * This file can be used by applications that need to communicate with the HSM
 * via the ioctl interface.
 */

#ifndef _UAPI_ACRN_H
#define _UAPI_ACRN_H

#include <linux/types.h>
#include <linux/uuid.h>

/**
 * struct acrn_vm_creation - Info to create a User VM
 * @vmid:		User VM ID returned from the hypervisor
 * @reserved0:		Reserved and must be 0
 * @vcpu_num:		Number of vCPU in the VM. Return from hypervisor.
 * @reserved1:		Reserved and must be 0
 * @uuid:		UUID of the VM. Pass to hypervisor directly.
 * @vm_flag:		Flag of the VM creating. Pass to hypervisor directly.
 * @ioreq_buf:		Service VM GPA of I/O request buffer. Pass to
 *			hypervisor directly.
 * @cpu_affinity:	CPU affinity of the VM. Pass to hypervisor directly.
 */
struct acrn_vm_creation {
	__u16	vmid;
	__u16	reserved0;
	__u16	vcpu_num;
	__u16	reserved1;
	guid_t	uuid;
	__u64	vm_flag;
	__u64	ioreq_buf;
	__u64	cpu_affinity;
};

struct acrn_gp_regs {
	__le64	rax;
	__le64	rcx;
	__le64	rdx;
	__le64	rbx;
	__le64	rsp;
	__le64	rbp;
	__le64	rsi;
	__le64	rdi;
	__le64	r8;
	__le64	r9;
	__le64	r10;
	__le64	r11;
	__le64	r12;
	__le64	r13;
	__le64	r14;
	__le64	r15;
};

struct acrn_descriptor_ptr {
	__le16	limit;
	__le64	base;
	__le16	reserved[3];
} __attribute__ ((__packed__));

struct acrn_regs {
	struct acrn_gp_regs		gprs;
	struct acrn_descriptor_ptr	gdt;
	struct acrn_descriptor_ptr	idt;

	__le64				rip;
	__le64				cs_base;
	__le64				cr0;
	__le64				cr4;
	__le64				cr3;
	__le64				ia32_efer;
	__le64				rflags;
	__le64				reserved_64[4];

	__le32				cs_ar;
	__le32				cs_limit;
	__le32				reserved_32[3];

	__le16				cs_sel;
	__le16				ss_sel;
	__le16				ds_sel;
	__le16				es_sel;
	__le16				fs_sel;
	__le16				gs_sel;
	__le16				ldt_sel;
	__le16				tr_sel;
};

/**
 * struct acrn_vcpu_regs - Info of vCPU registers state
 * @vcpu_id:	vCPU ID
 * @reserved:	Reserved and must be 0
 * @vcpu_regs:	vCPU registers state
 *
 * This structure will be passed to hypervisor directly.
 */
struct acrn_vcpu_regs {
	__u16			vcpu_id;
	__u16			reserved[3];
	struct acrn_regs	vcpu_regs;
};

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
 * @type:		Type of the memory mapping (ACRM_MEMMAP_*).
 *			Pass to hypervisor directly.
 * @attr:		Attribute of the memory mapping.
 *			Pass to hypervisor directly.
 * @user_vm_pa:		Physical address of User VM.
 *			Pass to hypervisor directly.
 * @service_vm_pa:	Physical address of Service VM.
 *			Pass to hypervisor directly.
 * @vma_base:		VMA address of Service VM. Pass to hypervisor directly.
 * @len:		Length of the memory mapping.
 *			Pass to hypervisor directly.
 */
struct acrn_vm_memmap {
	__u32	type;
	__u32	attr;
	__u64	user_vm_pa;
	union {
		__u64	service_vm_pa;
		__u64	vma_base;
	};
	__u64	len;
};

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/*
 * Common IOCTL IDs definition for ACRN userspace
 */
#define ACRN_IOCTL_CREATE_VM		\
	_IOWR(ACRN_IOCTL_TYPE, 0x10, struct acrn_vm_creation)
#define ACRN_IOCTL_DESTROY_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x11)
#define ACRN_IOCTL_START_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x12)
#define ACRN_IOCTL_PAUSE_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x13)
#define ACRN_IOCTL_RESET_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x15)
#define ACRN_IOCTL_SET_VCPU_REGS	\
	_IOW(ACRN_IOCTL_TYPE, 0x16, struct acrn_vcpu_regs)

#define ACRN_IOCTL_SET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x41, struct acrn_vm_memmap)
#define ACRN_IOCTL_UNSET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x42, struct acrn_vm_memmap)

#endif /* _UAPI_ACRN_H */
