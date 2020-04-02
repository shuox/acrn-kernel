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

#endif /* __ACRN_HSM_HYPERCALL_H */
