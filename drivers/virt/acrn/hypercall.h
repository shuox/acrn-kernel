/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN HSM: hypercalls
 */
#ifndef _ACRN_HSM_HYPERCALL_H
#define _ACRN_HSM_HYPERCALL_H
#include <asm/acrn.h>

/*
 * Hypercall IDs of the ACRN hypervisor
 */
#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

#define HC_ID_GEN_BASE			0x0UL
#define HC_GET_API_VERSION		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)

/*
 * Get API_VERSION from hypervisor
 * @api_version: Service VM GPA of version info
 */
static inline long hcall_get_api_version(u64 api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

#endif /* __ACRN_HSM_HYPERCALL_H */
