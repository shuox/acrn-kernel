// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (HSM): driver-specific hypercall
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Jason Chen CJ <jason.cj.chen@intel.com>
 * 		Zhao Yakui <yakui.zhao@intel.com>
 * 		Jack Ren <jack.ren@intel.com>
 * 		Yin FengWei <fengwei.yin@intel.com>
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <asm/ptrace.h>
#include <asm/acrn.h>
#include "hypercall.h"

/* General */
long hcall_get_api_version(unsigned long api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

long hcall_sos_offline_cpu(unsigned long cpu)
{
	return acrn_hypercall1(HC_SOS_OFFLINE_CPU, cpu);
}

long hcall_get_platform_info(unsigned long platform_info)
{
	return acrn_hypercall1(HC_GET_PLATFORM_INFO, platform_info);
}

/* VM management */
long hcall_create_vm(unsigned long vminfo)
{
	return acrn_hypercall1(HC_CREATE_VM, vminfo);
}

long hcall_start_vm(unsigned long vmid)
{
	return  acrn_hypercall1(HC_START_VM, vmid);
}

long hcall_pause_vm(unsigned long vmid)
{
	return acrn_hypercall1(HC_PAUSE_VM, vmid);
}

long hcall_reset_vm(unsigned long vmid)
{
	return acrn_hypercall1(HC_RESET_VM, vmid);
}

long hcall_destroy_vm(unsigned long vmid)
{
	return acrn_hypercall1(HC_DESTROY_VM, vmid);
}

long hcall_create_vcpu(unsigned long vmid, unsigned long vcpu)
{
	return acrn_hypercall2(HC_CREATE_VCPU, vmid, vcpu);
}

long hcall_set_vcpu_regs(unsigned long vmid,
			 unsigned long regs_state)
{
	return acrn_hypercall2(HC_SET_VCPU_REGS, vmid, regs_state);
}

/* IRQ and Interrupts */
long hcall_inject_msi(unsigned long vmid, unsigned long msi)
{
	return acrn_hypercall2(HC_INJECT_MSI, vmid, msi);
}

long hcall_vm_intr_monitor(unsigned long vmid, unsigned long addr)
{
	return  acrn_hypercall2(HC_VM_INTR_MONITOR, vmid, addr);
}

long hcall_set_irqline(unsigned long vmid, unsigned long op)
{
	return acrn_hypercall2(HC_SET_IRQLINE, vmid, op);
}

/* DM ioreq management */
long hcall_set_ioreq_buffer(unsigned long vmid, unsigned long buffer)
{
	return acrn_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu)
{
	return acrn_hypercall2(HC_NOTIFY_REQUEST_FINISH, vmid, vcpu);
}

/* Guest memory management */
long hcall_set_memory_regions(unsigned long pa_regions)
{
	return acrn_hypercall1(HC_VM_SET_MEMORY_REGIONS, pa_regions);
}

long hcall_write_protect_page(unsigned long vmid, unsigned long wp)
{
	return acrn_hypercall2(HC_VM_WRITE_PROTECT_PAGE, vmid, wp);
}

/* PCI device assignment */
inline long hcall_assign_pcidev(unsigned long vmid, unsigned long addr)
{
	return acrn_hypercall2(HC_ASSIGN_PCIDEV, vmid, addr);
}

inline long hcall_deassign_pcidev(unsigned long vmid, unsigned long addr)
{
	return acrn_hypercall2(HC_DEASSIGN_PCIDEV, vmid, addr);
}

long hcall_set_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq)
{
	return acrn_hypercall2(HC_SET_PTDEV_INTR_INFO, vmid, pt_irq);
}

long hcall_reset_ptdev_intr_info(unsigned long vmid,
				 unsigned long pt_irq)
{
	return acrn_hypercall2(HC_RESET_PTDEV_INTR_INFO, vmid, pt_irq);
}

/* Power Management */
long hcall_get_cpu_state(unsigned long cmd, unsigned long state_pa)
{
	return acrn_hypercall2(HC_PM_GET_CPU_STATE, cmd, state_pa);
}
