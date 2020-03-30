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

#define HC_ID_VM_BASE			0x10UL
#define HC_CREATE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_RESET_VM			_HC_ID(HC_ID, HC_ID_VM_BASE + 0x05)
#define HC_SET_VCPU_REGS		_HC_ID(HC_ID, HC_ID_VM_BASE + 0x06)

#define HC_ID_IOREQ_BASE		0x30UL
#define HC_SET_IOREQ_BUFFER		_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH	_HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

#define HC_ID_MEM_BASE			0x40UL
#define HC_VM_SET_MEMORY_REGIONS	_HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)

/*
 * Get API_VERSION from hypervisor
 * @api_version: Service VM GPA of version info
 */
static inline long hcall_get_api_version(u64 api_version)
{
	return acrn_hypercall1(HC_GET_API_VERSION, api_version);
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

#endif /* __ACRN_HSM_HYPERCALL_H */
