/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN hyperviosr service module (HSM): driver-specific hypercall
 * (header definition)
 */
#ifndef _ACRN_HSM_HYPERCALL_H
#define _ACRN_HSM_HYPERCALL_H

/* General */
/* notify the hypervisor to offline one vcpu for SOS
 * cpu is the cpu number that needs to be offlined
 */
long hcall_sos_offline_cpu(unsigned long cpu);
/* return the API_VERSION of hypervisor
 * api_version points to the gpa of returned info
 */
long hcall_get_api_version(unsigned long api_version);
/* return the platform info of hypervisor
 * platform_info points to the gpa of returned info
 */
long hcall_get_platform_info(unsigned long platform_info);

/* VM management */
/* ask the hypervisor to create one Guest VM.
 * vminfo points to the gpa of created VM(in/out)
 */
long hcall_create_vm(unsigned long vminfo);
/* ask the hypervisor to start the given VM based on vmid.
 * vmid is the identifier for the given VM
 */
long hcall_start_vm(unsigned long vmid);
/* ask the hypervisor to pause the given VM based on vmid.
 * vmid is the identifier for the given VM
 */
long hcall_pause_vm(unsigned long vmid);
/* ask the hypervisor to release  the given VM based on vmid.
 * vmid is the identifier for the given VM
 */
long hcall_destroy_vm(unsigned long vmid);
/* ask the hypervisor to reset the given VM based on vmid.
 * vmid is the identifier for the given VM
 */
long hcall_reset_vm(unsigned long vmid);
/* ask the hypervisor to create one vcpu based on vmid.
 * vmid is the identifier for the given VM
 * vcpu is the cpu number that needs to be created
 */
long hcall_create_vcpu(unsigned long vmid, unsigned long vcpu);
/* ask the hypervisor to configure the regs_state for one vcpu in VM
 * vmid is the identifier for the given VM
 * regs_state points to the gpa of configured register state: cpu_id and
 *         register value.
 */
long hcall_set_vcpu_regs(unsigned long vmid, unsigned long regs_state);

/* IRQ and interrupt management */
/* notify the hypervisor to deliver MSI interrupt to target vm
 * vmid is the identifier of target VM
 * msi points to the gpa of MSI message
 */
long hcall_inject_msi(unsigned long vmid, unsigned long msi);
/* notify the hypervisor to query interrupt_count info for target VM
 * vmid is the identifier of target VM
 * addr is the GPA address that points to interrupt_count page of target VM
 */
long hcall_vm_intr_monitor(unsigned long vmid, unsigned long addr);
/* notify the hypervisor to handle the passed irq op
 * vmid is the identifier of target VM.
 * op is the defined irq op
 */
long hcall_set_irqline(unsigned long vmid, unsigned long op);

/* DM IOREQ management */
/* ask the hypervisor to setup the shared buffer for IO Request.
 * vmdi is the identifier of target VM
 * buffer points to the gpa address of the ioreq_buffer structure
 */
long hcall_set_ioreq_buffer(unsigned long vmid, unsigned long buffer);
/* notify that the ioreq on cpu of VMID is done
 * vmid is the identifier of target VM
 * cpu is the vCPU that triggers the iorequest
 */
long hcall_notify_req_finish(unsigned long vmid, unsigned long vcpu);

/* Guest Memory mamangement */
/* ask the hypervisor to setup EPT for the given VM.
 * pa_regions points to the gpa for memory_region that includes the
 * mapping between HPA and UOS GPA. The vmid is also included.
 */
long hcall_set_memory_regions(unsigned long pa_regions);

/* ask the hypervisor to enable/disable the EPT_WP for one 4K page on
 *    one given VM
 * vmid is the identifier of target VM
 * wp points to the gpa address that contains the wp_data structure
 */
long hcall_write_protect_page(unsigned long vmid, unsigned long wp);

/* PCI device assignement */
/* notify the hypervisor to assign one PCI device to target vm
 * vmid is the identifier of target VM
 * bdf is the assigned PCI device(bus:dev:func)
 */
long hcall_assign_ptdev(unsigned long vmid, unsigned long bdf);
/* notify the hypervisor to deassign one PCI device to target vm
 * vmid is the identifier of target VM
 * bdf is the deassigned PCI device(bus:dev:func)
 */
long hcall_deassign_ptdev(unsigned long vmid, unsigned long bdf);
/* notify the hypervisor to configure the interrupt_info for the assigned
 *        PCI device
 * vmid is the identifier of target VM
 * pt_irq is the GPA address that points to the pt_irq info
 */
long hcall_set_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq);
/* notify the hypervisor to reset the interrupt_info for the assigned
 *        PCI device
 * vmid is the identifier of target VM
 * pt_irq is the GPA address that points to the pt_irq info
 */
long hcall_reset_ptdev_intr_info(unsigned long vmid, unsigned long pt_irq);

/* Debug assignment */
/* TBD: It will be added when adding debug module */

/* Power management */
/* get the cpu px/cx state from hypervisor.
 * state_pa points to the gpa of px/cx state buffer
 */
long hcall_get_cpu_state(unsigned long cmd, unsigned long state_pa);

#endif /* __ACRN_HSM_HYPERCALL_H */
