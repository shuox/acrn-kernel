/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_ioctl_defs.h
 *
 * ACRN definition for ioctl to user space
 */

#ifndef __ACRN_IOCTL_DEFS_H__
#define __ACRN_IOCTL_DEFS_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/**
 * struct api_version - data structure to track ACRN_SRV API version
 *
 * @major_version: major version of ACRN_SRV API
 * @minor_version: minor version of ACRN_SRV API
 */
struct api_version {
	uint32_t major_version;
	uint32_t minor_version;
};

/**
 * @brief Info to create a VM, the parameter for HC_CREATE_VM hypercall
 */
struct acrn_create_vm {
	/** created vmid return to ACRN. Keep it first field */
	uint16_t vmid;

	/** Reserved */
	uint16_t reserved0;

	/** VCPU numbers this VM want to create */
	uint16_t vcpu_num;

	/** Reserved */
	uint16_t reserved1;

	/** the GUID of this VM */
	uint8_t	 GUID[16];

	/* VM flag bits from Guest OS. */
	uint64_t vm_flag;

	/** guest physical address of VM request_buffer */
	uint64_t req_buf;

	/** Reserved for future use*/
	uint8_t  reserved2[16];
};

/**
 * @brief Info to create a VCPU
 *
 * the parameter for HC_CREATE_VCPU hypercall
 */
struct acrn_create_vcpu {
	/** the virtual CPU ID for the VCPU created */
	uint16_t vcpu_id;

	/** the physical CPU ID for the VCPU created */
	uint16_t pcpu_id;

	/** Reserved for future use*/
	uint8_t reserved[4];
} __aligned(8);

struct acrn_gp_regs {
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

struct acrn_descriptor_ptr {
	uint16_t limit;
	uint64_t base;
	uint16_t reserved[3];
} __packed;

struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;
	struct acrn_descriptor_ptr gdt;
	struct acrn_descriptor_ptr idt;

	uint64_t        rip;
	uint64_t        cs_base;
	uint64_t        cr0;
	uint64_t        cr4;
	uint64_t        cr3;
	uint64_t        ia32_efer;
	uint64_t        rflags;
	uint64_t        reserved_64[4];

	uint32_t        cs_ar;
	uint32_t        reserved_32[4];

	/* don't change the order of following sel */
	uint16_t        cs_sel;
	uint16_t        ss_sel;
	uint16_t        ds_sel;
	uint16_t        es_sel;
	uint16_t        fs_sel;
	uint16_t        gs_sel;
	uint16_t        ldt_sel;
	uint16_t        tr_sel;

	uint16_t        reserved_16[4];
};

/**
 * @brief Info to set vcpu state
 *
 * the pamameter for HC_SET_VCPU_REGS
 */
struct acrn_set_vcpu_regs {
	/** the virtual CPU ID for the VCPU */
	uint16_t vcpu_id;

	/** reserved space to make cpu_state aligned to 8 bytes */
	uint16_t reserved0[3];

	/** the structure to hold vcpu state */
	struct acrn_vcpu_regs vcpu_regs;
};

/*
 * Common IOCTL ID definition for DM
 */
#define _IC_ID(x, y) (((x) << 24) | (y))
#define IC_ID 0x43UL

/* General */
#define IC_ID_GEN_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_GEN_BASE + 0x00)

/* VM management */
#define IC_ID_VM_BASE                  0x10UL
#define IC_CREATE_VM                   _IC_ID(IC_ID, IC_ID_VM_BASE + 0x00)
#define IC_DESTROY_VM                  _IC_ID(IC_ID, IC_ID_VM_BASE + 0x01)
#define IC_START_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x02)
#define IC_PAUSE_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x03)
#define IC_CREATE_VCPU                 _IC_ID(IC_ID, IC_ID_VM_BASE + 0x04)
#define IC_RESET_VM                    _IC_ID(IC_ID, IC_ID_VM_BASE + 0x05)
#define IC_SET_VCPU_REGS               _IC_ID(IC_ID, IC_ID_VM_BASE + 0x06)

#endif /* __ACRN_IOCTL_DEFS_H__ */
