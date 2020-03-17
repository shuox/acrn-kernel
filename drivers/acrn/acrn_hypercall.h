/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN hyperviosr service module (HSM): driver-specific hypercall
 * (header definition)
 */
#ifndef _ACRN_HSM_HYPERCALL_H
#define _ACRN_HSM_HYPERCALL_H

/* General */
/*
 * Notify the hypervisor to offline one vcpu of host
 * @cpu: cpu to be offlined
 */
long hcall_sos_offline_cpu(unsigned long cpu);

/*
 * Get API_VERSION from hypervisor
 * @api_version: physical address of version info
 */
long hcall_get_api_version(unsigned long api_version);

/*
 * Get platform info from hypervisor
 * @platform_info: physical address of platform info
 */
long hcall_get_platform_info(unsigned long platform_info);

/* VM management */
/*
 * Create VM
 * @vminfo: physical address of created VM info(in/out)
 */
long hcall_create_vm(unsigned long vminfo);

/*
 * Start VM
 * @vmid: identifier of target VM
 */
long hcall_start_vm(unsigned long vmid);

/*
 * Pause VM
 * @vmid: identifier of target VM
 */
long hcall_pause_vm(unsigned long vmid);

/*
 * Destroy VM
 * @vmid: identifier of target VM
 */
long hcall_destroy_vm(unsigned long vmid);

/*
 * Reset VM
 * @vmid: identifier of target VM
 */
long hcall_reset_vm(unsigned long vmid);

/* 
 * Setup registers of vCPU
 * @vmid: identifier of target VM
 * @regs_state: HPA of paramters for registers setup
 */
long hcall_set_vcpu_regs(unsigned long vmid, unsigned long regs_state);

/* Interrupt management */
/*
 * Deliver a MSI interrupt to target VM through hypervisor
 * @vmid: identifier of target VM
 * @msi: HPA of MSI message
 */
long hcall_inject_msi(unsigned long vmid, unsigned long msi);

/*
 * Get target VM's interrupt statistics
 * @vmid: identifier of target VM
 * @msi: HPA of page to store interrupt statistics
 */
long hcall_vm_intr_monitor(unsigned long vmid, unsigned long addr);

/*
 * Set or clear a IRQ line
 * @vmid: identifier of target VM
 * @op: HPA of defined irq op
 */
long hcall_set_irqline(unsigned long vmid, unsigned long op);

/* IO request management */
/*
 * Setup the shared buffer for IO Request
 * @vmid: identifier of target VM
 * @buffer: HPA of ioreq_buffer structure
 */
long hcall_set_ioreq_buffer(unsigned long vmid, unsigned long buffer);

/*
 * Notify the ioreq originated from vcpu is done
 * @vmid: identifier of target VM
 * @vcpu: identifier of target VCPU
 */
long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu);

/* Guest Memory mamangement */
/*
 * Setup EPT mapping for the given VM
 * @pa_regions: HPA for memory_region that includes the mapping
 * 		between HPA and Guest GPA. The vmid is also included.
 */
long hcall_set_memory_regions(unsigned long pa_regions);

/*
 * Enable or Disable the EPT_WP for one 4K page on given VM
 * @vmid: identifier of target VM
 * @wp: HPA that contains the wp_data structure
 */
long hcall_write_protect_page(unsigned long vmid, unsigned long wp);

/* PCI device assignement */
/*
 * Assign one PCI device to target VM
 * @vmid: identifier of target VM
 * @bdf: the assigned PCI device(bus:dev:func)
 */
long hcall_assign_ptdev(unsigned long vmid, unsigned long bdf);

/*
 * Deassign one PCI device from target VM
 * @vmid: identifier of target VM
 * @bdf: the assigned PCI device(bus:dev:func)
 */
long hcall_deassign_ptdev(unsigned long vmid, unsigned long bdf);

/*
 * Configure interrupt for the assigned PCI device
 * @vmid: identifier of target VM
 * @pt_irq: HPA of pt_irq structure
 */
long hcall_set_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq);

/*
 * Reset interrupt for the assigned PCI device
 * @vmid: identifier of target VM
 * @pt_irq: HPA of pt_irq structure
 */
long hcall_reset_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq);

/* Power management */
/*
 * Get the cpu P-states and C-states info from hypervisor
 * @state_pa: HPA of P-states and C-states buffer
 */
long hcall_get_cpu_state(unsigned long cmd, unsigned long state_pa);

#endif /* __ACRN_HSM_HYPERCALL_H */
