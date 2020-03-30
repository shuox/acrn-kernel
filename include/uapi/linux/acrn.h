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

#endif /* _UAPI_ACRN_H */
