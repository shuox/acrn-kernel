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

#endif /* _ACRN_H */
