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
