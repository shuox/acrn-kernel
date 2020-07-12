/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * ACRN HSM: hypercalls
 */
#ifndef __ACRN_HSM_HYPERCALL_H
#define __ACRN_HSM_HYPERCALL_H
#include <asm/acrn.h>

/*
 * Hypercall IDs of the ACRN Hypervisor
 */
#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

#define HC_ID_GEN_BASE			0x0UL
#define HC_GET_API_VERSION		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)

/**
 * hcall_get_api_version() - Get API version from hypervisor
 * @api_version: Service VM GPA of version info
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_get_api_version(u64 api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

#endif /* __ACRN_HSM_HYPERCALL_H */
