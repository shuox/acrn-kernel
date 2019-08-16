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

/*
 * Common IOCTL ID definition for DM
 */
#define _IC_ID(x, y) (((x) << 24) | (y))
#define IC_ID 0x43UL

/* General */
#define IC_ID_GEN_BASE                  0x0UL
#define IC_GET_API_VERSION             _IC_ID(IC_ID, IC_ID_GEN_BASE + 0x00)

#endif /* __ACRN_IOCTL_DEFS_H__ */
