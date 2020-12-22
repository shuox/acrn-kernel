// SPDX-License-Identifier: GPL-2.0
/*
 * ACRN detection support
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 *
 */

#include <linux/interrupt.h>

static void (*acrn_intr_handler)(void);
static int acrn_hv_intr = 60;

static irqreturn_t acrn_hv_callback(int irq, void *arg)
{
	if (acrn_intr_handler)
		acrn_intr_handler();
	return IRQ_HANDLED;
}

void acrn_setup_intr_handler(void (*handler)(void))
{
	acrn_intr_handler = handler;
}
EXPORT_SYMBOL_GPL(acrn_setup_intr_handler);

void acrn_remove_intr_handler(void)
{
	acrn_intr_handler = NULL;
}
EXPORT_SYMBOL_GPL(acrn_remove_intr_handler);

static int __init acrn_guest_init(void)
{
	return request_percpu_irq(acrn_hv_intr, acrn_hv_callback, "acrn_hv", NULL);
}
early_initcall(acrn_guest_init);
