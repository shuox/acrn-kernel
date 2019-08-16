/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_ACRN_H
#define _ASM_X86_ACRN_H

extern void acrn_setup_intr_irq(void (*handler)(void));
extern void acrn_remove_intr_irq(void);

#endif /* _ASM_X86_ACRN_H */
