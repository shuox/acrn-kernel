/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/**
 * @file acrn_hsm.h
 *
 * ACRN HSM APIs for other modules in the Service VM.
 */

#ifndef _ACRN_HSM_H
#define _ACRN_HSM_H

#include <linux/types.h>
#include <linux/acrn.h>

struct acrn_vm;

void *acrn_mm_gpa2sva(struct acrn_vm *vm, u64 user_gpa, size_t size);
int acrn_mm_add_region(u16 vmid, u64 user_gpa, u64 service_gpa, u64 size,
			   u32 mem_type, u32 mem_access_right);
int acrn_mm_del_region(u16 vmid, u64 user_gpa, u64 size);

#endif
