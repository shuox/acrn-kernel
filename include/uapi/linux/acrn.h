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

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/**
 * Common IOCTL IDs definition for ACRN userspace
 */
#define ACRN_IOCTL_GET_API_VERSION	\
	_IOR(ACRN_IOCTL_TYPE, 0, struct acrn_api_version)

#endif /* _ACRN_H */
