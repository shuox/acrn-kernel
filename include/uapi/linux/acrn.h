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

#endif /* _UAPI_ACRN_H */
