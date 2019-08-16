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

/*
 * Hypercalls for ACRN
 *
 * - VMCALL instruction is used to implement ACRN hypercalls.
 * - ACRN hypercall ABI:
 *   - Hypercall number is passed in R8 register.
 *   - Up to 2 arguments are passed in RDI, RSI.
 *   - Return value will be placed in RAX.
 */
static inline long acrn_hypercall0(unsigned long hcall_id)
{
	long result;

	/*
	 * Use explicit MOV because gcc doesn't support R8
	 * as direct register constraints.
	 */
	asm volatile("movq %[hcall_id], %%r8\n\t"
		     "vmcall\n\t"
		     : "=a" (result)
		     : [hcall_id] "g" (hcall_id)
		     : "r8");

	return result;
}

static inline long acrn_hypercall1(unsigned long hcall_id,
				   unsigned long param1)
{
	long result;

	/*
	 * Use explicit MOV because gcc doesn't support R8
	 * as direct register constraints.
	 */
	asm volatile("movq %[hcall_id], %%r8\n\t"
		     "vmcall\n\t"
		     : "=a" (result)
		     : [hcall_id] "g" (hcall_id), "D" (param1)
		     : "r8");

	return result;
}

static inline long acrn_hypercall2(unsigned long hcall_id,
				   unsigned long param1,
				   unsigned long param2)
{
	long result;

	/*
	 * Use explicit MOV because gcc doesn't support R8
	 * as direct register constraints.
	 */
	asm volatile("movq %[hcall_id], %%r8\n\t"
		     "vmcall\n\t"
		     : "=a" (result)
		     : [hcall_id] "g" (hcall_id), "D" (param1), "S" (param2)
		     : "r8");

	return result;
}

#endif /* _ASM_X86_ACRN_H */
