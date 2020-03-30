/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */

#ifndef __ACRN_HSM_DRV_H
#define __ACRN_HSM_DRV_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/acrn.h>

#include "hypercall.h"

#define ACRN_INVALID_VMID (0xffffU)

struct acrn_vm {
	/* VM ID */
	u16	vmid;
};

#endif
