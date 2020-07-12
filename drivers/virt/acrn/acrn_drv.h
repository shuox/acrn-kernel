/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/acrn.h>
#include <linux/types.h>

#include "hypercall.h"

#define ACRN_INVALID_VMID (0xffffU)

/**
 * struct acrn_vm - Structure of ACRN VM.
 * @vmid: VM ID
 */
struct acrn_vm {
	u16	vmid;
};

#endif /* __ACRN_HSM_DRV_H */
