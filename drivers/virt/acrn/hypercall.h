/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * ACRN HSM: hypercalls
 */
#ifndef _ACRN_HSM_HYPERCALL_H
#define _ACRN_HSM_HYPERCALL_H
#include <asm/acrn.h>

/*
 * Hypercall IDs of the ACRN hypervisor
 */
#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

#define HC_ID_GEN_BASE			0x0UL
#define HC_GET_API_VERSION		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)
#define HC_SOS_OFFLINE_CPU		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x01)

#define HC_ID_VM_BASE			0x10UL
#define HC_CREATE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_RESET_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)
#define HC_SET_VCPU_REGS		_HC_ID(HC_ID, HC_ID_VM_BASE + 0x06)

#define HC_ID_IRQ_BASE			0x20UL
#define HC_INJECT_MSI			_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)
#define HC_VM_INTR_MONITOR		_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x04)
#define HC_SET_IRQLINE			_HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x05)

#define HC_ID_IOREQ_BASE		0x30UL
#define HC_SET_IOREQ_BUFFER		_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH	_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

#define HC_ID_MEM_BASE			0x40UL
#define HC_VM_SET_MEMORY_REGIONS	_HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)

#define HC_ID_PCI_BASE			0x50UL
#define HC_SET_PTDEV_INTR		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)
#define HC_ASSIGN_PCIDEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x05)
#define HC_DEASSIGN_PCIDEV		_HC_ID(HC_ID, HC_ID_PCI_BASE + 0x06)

#define HC_ID_PM_BASE			0x80UL
#define HC_PM_GET_CPU_STATE		_HC_ID(HC_ID, HC_ID_PM_BASE + 0x00)

/*
 * Get API_VERSION from hypervisor
 * @api_version: Service VM GPA of version info
 */
static inline long hcall_get_api_version(u64 api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

/*
 * Notify the hypervisor to offline a vCPU of the Service VM
 * @cpu: the offline cpu
 */
static inline long hcall_sos_offline_cpu(u64 cpu)
{
	return acrn_hypercall1(HC_SOS_OFFLINE_CPU, cpu);
}

/*
 * Create VM
 * @vminfo: Service VM GPA of info of VM creation
 */
static inline long hcall_create_vm(u64 vminfo)
{
	return acrn_hypercall1(HC_CREATE_VM, vminfo);
}

/*
 * Start VM
 * @vmid: VM ID
 */
static inline long hcall_start_vm(u64 vmid)
{
	return  acrn_hypercall1(HC_START_VM, vmid);
}

/*
 * Pause VM
 * @vmid: VM ID
 */
static inline long hcall_pause_vm(u64 vmid)
{
	return acrn_hypercall1(HC_PAUSE_VM, vmid);
}

/*
 * Destroy VM
 * @vmid: VM ID
 */
static inline long hcall_destroy_vm(u64 vmid)
{
	return acrn_hypercall1(HC_DESTROY_VM, vmid);
}

/*
 * Reset VM
 * @vmid: VM ID
 */
static inline long hcall_reset_vm(u64 vmid)
{
	return acrn_hypercall1(HC_RESET_VM, vmid);
}

/*
 * Set up registers of vBSP of the VM
 * @vmid: VM ID
 * @regs_state: Service VM GPA of registers state
 */
static inline long hcall_set_vcpu_regs(u64 vmid, u64 regs_state)
{
	return acrn_hypercall2(HC_SET_VCPU_REGS, vmid, regs_state);
}

/*
 * Deliver a MSI interrupt to a User VM
 * @vmid: The VM ID of User VM
 * @msi: Service VM GPA of MSI message
 */
static inline long hcall_inject_msi(u64 vmid, u64 msi)
{
	return acrn_hypercall2(HC_INJECT_MSI, vmid, msi);
}

/*
 * Set a shared page for interrupt statistics of a User VM
 * @vmid: The VM ID of User VM
 * @msi: Service VM GPA of the shared page
 */
static inline long hcall_vm_intr_monitor(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_VM_INTR_MONITOR, vmid, addr);
}

/*
 * Set or clear an interrupt line
 * @vmid: The VM ID of User VM
 * @op: Service VM GPA of interrupt line operations
 */
static inline long hcall_set_irqline(u64 vmid, u64 op)
{
	return acrn_hypercall2(HC_SET_IRQLINE, vmid, op);
}

/*
 * Set up the shared buffer for I/O Requests
 * @vmid: VM ID
 * @buffer: Service VM GPA of the shared buffer
 */
static inline long hcall_set_ioreq_buffer(u64 vmid, u64 buffer)
{
	return acrn_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

/*
 * Notify the ACRN hypervisor of an I/O request completion
 * @vmid: VM ID
 * @vcpu: the vCPU which initiated the I/O request
 */
static inline long hcall_notify_req_finish(u64 vmid, u64 vcpu)
{
	return acrn_hypercall2(HC_NOTIFY_REQUEST_FINISH, vmid, vcpu);
}

/*
 * EPT mapping setup
 * @regions_pa: Service VM GPA for vm_memory_region_list that includes the
 *		addresses of Service VM GPA and User GPA.
 */
static inline long hcall_set_memory_regions(u64 regions_pa)
{
	return acrn_hypercall1(HC_VM_SET_MEMORY_REGIONS, regions_pa);
}

/*
 * Assign a PCI device to a User VM
 * @vmid: The VM ID of User VM
 * @addr: Service VM GPA of the acrn_pcidev structure
 */
static inline long hcall_assign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_ASSIGN_PCIDEV, vmid, addr);
}

/*
 * De-assign a PCI device from a User VM
 * @vmid: The VM ID of User VM
 * @addr: Service VM GPA of the acrn_pcidev structure
 */
static inline long hcall_deassign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_DEASSIGN_PCIDEV, vmid, addr);
}

/*
 * Configure a interrupt for the assigned PCI device
 * @vmid: The VM ID of User VM
 * @irq: Service VM GPA of the acrn_ptdev_irq structure
 */
static inline long hcall_set_ptdev_intr(u64 vmid, u64 irq)
{
	return acrn_hypercall2(HC_SET_PTDEV_INTR, vmid, irq);
}

/*
 * Re-configure a interrupt for the assigned PCI device
 * @vmid: The VM ID of User VM
 * @irq: Service VM GPA of the acrn_ptdev_irq structure
 */
static inline long hcall_reset_ptdev_intr(u64 vmid, u64 irq)
{
	return acrn_hypercall2(HC_RESET_PTDEV_INTR, vmid, irq);
}

/*
 * Get cpu P-states and C-states info from the hypervisor
 * @state: Service VM GPA of buffer of P-states and C-states
 */
static inline long hcall_get_cpu_state(u64 cmd, u64 state)
{
	return acrn_hypercall2(HC_PM_GET_CPU_STATE, cmd, state);
}

#endif /* __ACRN_HSM_HYPERCALL_H */
