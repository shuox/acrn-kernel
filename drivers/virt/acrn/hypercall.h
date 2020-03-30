/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * ACRN HSM: hypercalls
 */
#ifndef __ACRN_HSM_HYPERCALL_H
#define __ACRN_HSM_HYPERCALL_H
#include <asm/acrn.h>

/*
 * Hypercall IDs of the ACRN Hypervisor
 */
#define _HC_ID(x, y) (((x) << 24) | (y))

#define HC_ID 0x80UL

#define HC_ID_GEN_BASE			0x0UL
#define HC_GET_API_VERSION		_HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)

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

/**
 * hcall_get_api_version() - Get API version from hypervisor
 * @api_version: Service VM GPA of version info
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_get_api_version(u64 api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
}

/**
 * hcall_create_vm() - Create VM
 * @vminfo: Service VM GPA of info of VM creation
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_create_vm(u64 vminfo)
{
	return acrn_hypercall1(HC_CREATE_VM, vminfo);
}

/**
 * hcall_start_vm() - Start VM
 * @vmid: VM ID
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_start_vm(u64 vmid)
{
	return acrn_hypercall1(HC_START_VM, vmid);
}

/**
 * hcall_pause_vm() - Pause VM
 * @vmid: VM ID
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_pause_vm(u64 vmid)
{
	return acrn_hypercall1(HC_PAUSE_VM, vmid);
}

/**
 * hcall_destroy_vm() - Destroy VM
 * @vmid: VM ID
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_destroy_vm(u64 vmid)
{
	return acrn_hypercall1(HC_DESTROY_VM, vmid);
}

/**
 * hcall_reset_vm() - Reset VM
 * @vmid: VM ID
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_reset_vm(u64 vmid)
{
	return acrn_hypercall1(HC_RESET_VM, vmid);
}

/**
 * hcall_set_vcpu_regs() - Set up registers of virtual BSP of the VM
 * @vmid: VM ID
 * @regs_state: Service VM GPA of registers state
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_set_vcpu_regs(u64 vmid, u64 regs_state)
{
	return acrn_hypercall2(HC_SET_VCPU_REGS, vmid, regs_state);
}

/**
 * hcall_inject_msi() - Deliver a MSI interrupt to a User VM.
 * @vmid: The VM ID of User VM.
 * @msi: Service VM GPA of MSI message.
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_inject_msi(u64 vmid, u64 msi)
{
	return acrn_hypercall2(HC_INJECT_MSI, vmid, msi);
}

/**
 * hcall_vm_intr_monitor() - Set a shared page for User VM interrupt statistics.
 * @vmid: The VM ID of User VM.
 * @addr: Service VM GPA of the shared page.
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_vm_intr_monitor(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_VM_INTR_MONITOR, vmid, addr);
}

/**
 * hcall_set_irqline() - Set or clear an interrupt line.
 * @vmid: The VM ID of User VM.
 * @op: Service VM GPA of interrupt line operations.
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_set_irqline(u64 vmid, u64 op)
{
	return acrn_hypercall2(HC_SET_IRQLINE, vmid, op);
}

/**
 * hcall_set_ioreq_buffer() - Set up the shared buffer for I/O Requests.
 * @vmid: VM ID.
 * @buffer: Service VM GPA of the shared buffer.
 */
static inline s64 hcall_set_ioreq_buffer(u64 vmid, u64 buffer)
{
	return acrn_hypercall2(HC_SET_IOREQ_BUFFER, vmid, buffer);
}

/**
 * hcall_notify_req_finish() - Notify ACRN Hypervisor of I/O request completion.
 * @vmid: VM ID.
 * @vcpu: The vCPU which initiated the I/O request.
 */
static inline s64 hcall_notify_req_finish(u64 vmid, u64 vcpu)
{
	return acrn_hypercall2(HC_NOTIFY_REQUEST_FINISH, vmid, vcpu);
}

/**
 * hcall_set_memory_regions() - EPT mapping setup
 * @regions_pa: Service VM GPA for vm_memory_region_list that includes the
 *		addresses of Service VM GPA and User GPA.
 *
 * Return: 0 on success, <0 on failure
 */
static inline s64 hcall_set_memory_regions(u64 regions_pa)
{
	return acrn_hypercall1(HC_VM_SET_MEMORY_REGIONS, regions_pa);
}

/**
 * hcall_assign_pcidev() - Assign a PCI device to a User VM.
 * @vmid: The VM ID of User VM.
 * @addr: Service VM GPA of the acrn_pcidev structure.
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_assign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_ASSIGN_PCIDEV, vmid, addr);
}

/**
 * hcall_deassign_pcidev() - De-assign a PCI device from a User VM.
 * @vmid: The VM ID of User VM.
 * @addr: Service VM GPA of the acrn_pcidev structure.
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_deassign_pcidev(u64 vmid, u64 addr)
{
	return acrn_hypercall2(HC_DEASSIGN_PCIDEV, vmid, addr);
}

/**
 * hcall_set_ptdev_intr() - Configure an interrupt for an assigned PCI device.
 * @vmid: The VM ID of User VM
 * @irq: Service VM GPA of the acrn_ptdev_irq structure
 *
 * Return: 0 on success, <0 on failure
 */
static inline long hcall_set_ptdev_intr(u64 vmid, u64 irq)
{
	return acrn_hypercall2(HC_SET_PTDEV_INTR, vmid, irq);
}

/**
 * hcall_reset_ptdev_intr() - Reset an interrupt for an assigned PCI device.
 * @vmid: The VM ID of User VM
 * @irq: Service VM GPA of the acrn_ptdev_irq structure
 *
 * Return: 0 on success, <0 on failure
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
