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
extern bool acrn_is_privileged_vm(void);

/*
 * Hypercalls for ACRN guest
 *
 * - ACRN only allows 64bit guest make hypercall.
 * - VMCALL instruction is used to implement ACRN hypercall.
 * - This is ACRN hypercall ABI:
 *   - Hypercall number is passed in R8 register.
 *   - Up to 2 arguments are passed in RDI, RSI.
 *   - Return value will be placed in RAX.
 *
 * NOTE: For ACRN hypercalls, the hypercall number need be
 *       be accessed fast, we use register for it.
 *
 *       For some ACRN hypercalls, they need access relative
 *       guest id fast, we use second register for it.
 *
 *       Other info will be passed to hypervisor by guest memory
 *       copy. We use third register for guest memory address.
 */
static inline long acrn_hypercall0(unsigned long hcall_id)
{
	long result;

	/*
	 * The hypercall is implemented with the VMCALL instruction.
	 * volatile qualifier is added to avoid that it is dropped
	 * because of compiler optimization.
	 *
	 * "movq" is explicitly used to emphasize that ACRN only allows
	 * 64bit guest make hypercall when "mov" also work here.
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

	/* See comments in acrn_hypercall0 */
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

	/* See comments in acrn_hypercall0 */
	asm volatile("movq %[hcall_id], %%r8\n\t"
		     "vmcall\n\t"
		     : "=a" (result)
		     : [hcall_id] "g" (hcall_id), "D" (param1), "S" (param2)
		     : "r8");

	return result;
}

#endif /* _ASM_X86_ACRN_H */
