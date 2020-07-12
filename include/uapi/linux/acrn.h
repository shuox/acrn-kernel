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

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/*
 * Common IOCTL IDs definition for ACRN userspace
 */
#define ACRN_IOCTL_GET_API_VERSION	\
	_IOR(ACRN_IOCTL_TYPE, 0, struct acrn_api_version)

#endif /* _UAPI_ACRN_H */
