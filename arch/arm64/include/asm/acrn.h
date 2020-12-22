/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_ARM64_ACRN_H
#define _ASM_ARM64_ACRN_H

void acrn_setup_intr_handler(void (*handler)(void));
void acrn_remove_intr_handler(void);

/*
 * Hypercalls for ACRN ARM
 *
 * - VMCALL instruction is used to implement ACRN hypercalls.
 * - ACRN hypercall ABI on ARM:
 *   - HVC immediate 0x1 (to distinguish with xen fast call)
 *   - Hypercall number is passed in x16 register.
 *   - Up to 2 arguments are passed in x0, x1.
 *   - Return value will be placed in x0.
 *
 */
static inline long acrn_hypercall0(unsigned long hcall_id)
{
	register long *result asm ("x0");

	asm volatile("mov x16, %1\n"
		     "hvc #0x1\n"
		     : "=r" (result)
		     : "r" (hcall_id)
		     : "x16", "memory");

	return result;
}

static inline long acrn_hypercall1(unsigned long hcall_id,
				   unsigned long param1)
{
	register long *result asm ("x0");

	asm volatile("mov x16, %1\n"
		     "mov x0, %2\n"
		     "hvc #0x1\n"
		     : "=r" (result)
		     : "r" (hcall_id), "r" (param1)
		     : "x16", "memory");

	return result;
}

static inline long acrn_hypercall2(unsigned long hcall_id,
				   unsigned long param1,
				   unsigned long param2)
{
	register long *result asm ("x0");

	asm volatile("mov x16, %1\n"
		     "mov x0, %2\n"
		     "mov x1, %3\n"
		     "hvc #0x1\n"
		     : "=r" (result)
		     : "r" (hcall_id), "r" (param1), "r" (param2)
		     : "x16", "x1", "memory");

	return result;
}

#endif
