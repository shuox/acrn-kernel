/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_common_def.h
 *
 * Common structure/definitions for acrn_ioctl/acrn_drv
 */

#ifndef _ACRN_COMMON_DEF_H
#define _ACRN_COMMON_DEF_H

/* Generic memory attributes */
#define	MEM_ACCESS_READ                 0x00000001
#define	MEM_ACCESS_WRITE                0x00000002
#define	MEM_ACCESS_EXEC	                0x00000004
#define	MEM_ACCESS_RWX			(MEM_ACCESS_READ | MEM_ACCESS_WRITE | \
						MEM_ACCESS_EXEC)
#define MEM_ACCESS_RIGHT_MASK           0x00000007
#define	MEM_TYPE_WB                     0x00000040
#define	MEM_TYPE_WT                     0x00000080
#define	MEM_TYPE_UC                     0x00000100
#define	MEM_TYPE_WC                     0x00000200
#define	MEM_TYPE_WP                     0x00000400
#define MEM_TYPE_MASK                   0x000007C0

#endif /* _ACRN_COMMON_DEF_H */
