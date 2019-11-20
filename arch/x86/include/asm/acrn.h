/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_ACRN_H
#define _ASM_X86_ACRN_H

/*
 * This CPUID returns feature bitmaps in EAX.
 * Guest VM uses this to detect the appropriate feature bit.
 */
#define	ACRN_CPUID_FEATURES	0x40000001
/* Bit 0 indicates whether guest VM is privileged */
#define	ACRN_FEATURE_PRIVILEGED_VM	BIT(0)

extern void acrn_setup_intr_irq(void (*handler)(void));
extern void acrn_remove_intr_irq(void);
extern bool acrn_is_privileged_vm(void);
#endif /* _ASM_X86_ACRN_H */
