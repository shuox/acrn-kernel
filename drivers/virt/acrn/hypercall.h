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

#endif /* __ACRN_HSM_HYPERCALL_H */
