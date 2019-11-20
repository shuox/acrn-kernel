/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_ACRN_H
#define _ASM_X86_ACRN_H

/*
 * This CPUID returns feature bitmaps in eax. Guest could
 * use this to detect the appropriate feature bit.
 */
#define	ACRN_CPUID_FEATURES	0x40000001
/* This means the guest is a privilege guest */
#define	ACRN_FEATURE_PRIVILEGE_VM	0

extern void acrn_hv_callback_vector(void);
#ifdef CONFIG_TRACING
#define trace_acrn_hv_callback_vector acrn_hv_callback_vector
#endif

extern void acrn_hv_vector_handler(struct pt_regs *regs);

extern void acrn_setup_intr_irq(void (*handler)(void));
extern void acrn_remove_intr_irq(void);
extern bool acrn_is_privilege_vm(void);
#endif /* _ASM_X86_ACRN_H */
