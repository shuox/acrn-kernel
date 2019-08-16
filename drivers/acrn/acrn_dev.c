// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (HSM): main framework
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Liu Shuo <shuo.a.liu@intel.com>
 * Zhao Yakui <yakui.zhao@intel.com>
 */

#include <linux/bits.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <asm/acrn.h>
#include <asm/hypervisor.h>
#include <linux/acrn/acrn_ioctl_defs.h>

#include "acrn_hypercall.h"
#include "acrn_drv_internal.h"

#define	DEVICE_NAME	"acrn_hsm"

static struct api_version acrn_api_version;

static
int acrn_dev_open(struct inode *inodep, struct file *filep)
{
	struct acrn_vm *vm;
	int i;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	refcount_set(&vm->refcnt, 1);
	vm->vmid = ACRN_INVALID_VMID;
	vm->dev = acrn_device;

	for (i = 0; i < HUGEPAGE_HLIST_ARRAY_SIZE; i++)
		INIT_HLIST_HEAD(&vm->hugepage_hlist[i]);
	mutex_init(&vm->hugepage_lock);

	write_lock_bh(&acrn_vm_list_lock);
	vm_list_add(&vm->list);
	write_unlock_bh(&acrn_vm_list_lock);
	filep->private_data = vm;

	pr_info("%s: opening device node\n", __func__);
	return 0;
}

static
long acrn_dev_ioctl(struct file *filep,
		    unsigned int ioctl_num, unsigned long ioctl_param)
{
	struct acrn_vm *vm;

	vm = (struct acrn_vm *)filep->private_data;
	if (!vm) {
		pr_err("acrn: invalid VM !\n");
		return -EFAULT;
	}
 
	if (ioctl_num == IC_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param, &acrn_api_version,
				 sizeof(acrn_api_version)))
			return -EFAULT;
	}

	if ((vm->vmid == ACRN_INVALID_VMID) && (ioctl_num != IC_CREATE_VM)) {
		pr_err("acrn: invalid VM ID for IOCTL %x!\n", ioctl_num);
		return -EFAULT;
	}

	switch (ioctl_num) {
	case IC_CREATE_VM: {
		struct acrn_create_vm *created_vm;

		created_vm = kmalloc(sizeof(*created_vm), GFP_KERNEL);
		if (!created_vm)
			return -ENOMEM;

		if (copy_from_user(created_vm, (void __user *)ioctl_param,
				   sizeof(struct acrn_create_vm))) {
			kfree(created_vm);
			return -EFAULT;
		}

		ret = hcall_create_vm(virt_to_phys(created_vm));
		if ((ret < 0) || (created_vm->vmid == ACRN_INVALID_VMID)) {
			pr_err("acrn: failed to create VM from Hypervisor !\n");
			kfree(created_vm);
			return -EFAULT;
		}

		if (copy_to_user((void __user *)ioctl_param, created_vm,
				 sizeof(struct acrn_create_vm))) {
			kfree(created_vm);
			return -EFAULT;
		}

		vm->vmid = created_vm->vmid;
		atomic_set(&vm->vcpu_num, 0);

		pr_debug("acrn: VM %d created\n", created_vm->vmid);
		kfree(created_vm);
		break;
	}

	case IC_START_VM: {
		ret = hcall_start_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: failed to start VM %d!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_PAUSE_VM: {
		ret = hcall_pause_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: failed to pause VM %d!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_RESET_VM: {
		ret = hcall_reset_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: failed to restart VM %d!\n", vm->vmid);
			return -EFAULT;
		}
		break;
	}

	case IC_DESTROY_VM: {
		ret = acrn_vm_destroy(vm);
		break;
	}

	case IC_CREATE_VCPU: {
		struct acrn_create_vcpu *cv;

		cv = kmalloc(sizeof(*cv), GFP_KERNEL);
		if (!cv)
			return -ENOMEM;

		if (copy_from_user(cv, (void __user *)ioctl_param,
				   sizeof(struct acrn_create_vcpu))) {
			kfree(cv);
			return -EFAULT;
		}

		ret = hcall_create_vcpu(vm->vmid, virt_to_phys(cv));
		if (ret < 0) {
			pr_err("acrn: failed to create vcpu %d for VM %d!\n",
			       cv->vcpu_id, vm->vmid);
			kfree(cv);
			return -EFAULT;
		}
		atomic_inc(&vm->vcpu_num);
		kfree(cv);

		return ret;
	}

	case IC_SET_VCPU_REGS: {
		struct acrn_set_vcpu_regs *cpu_regs;

		cpu_regs = kmalloc(sizeof(*cpu_regs), GFP_KERNEL);
		if (!cpu_regs)
			return -ENOMEM;

		if (copy_from_user(cpu_regs, (void __user *)ioctl_param,
				   sizeof(*cpu_regs))) {
			kfree(cpu_regs);
			return -EFAULT;
		}

		ret = hcall_set_vcpu_regs(vm->vmid, virt_to_phys(cpu_regs));
		kfree(cpu_regs);
		if (ret < 0) {
			pr_err("acrn: failed to set bsp state of vm %d!\n",
			       vm->vmid);
			return -EFAULT;
		}

		return ret;
	}

	case IC_SET_MEMSEG: {
		struct vm_memmap memmap;

		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = map_guest_memseg(vm, &memmap);
		break;
	}

	case IC_UNSET_MEMSEG: {
		struct vm_memmap memmap;

		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = unmap_guest_memseg(vm, &memmap);
		break;
	}

	default:
		pr_warn("Unknown IOCTL 0x%x\n", ioctl_num);
		ret = -EINVAL;
		break;
	}


	return 0;
}

static int acrn_dev_release(struct inode *inodep, struct file *filep)
{
	struct acrn_vm *vm = filep->private_data;

	if (!vm) {
		pr_err("acrn: invalid VM !\n");
		return -EFAULT;
	}
	if (vm->vmid != ACRN_INVALID_VMID)
		acrn_vm_destroy(vm);

	write_lock_bh(&acrn_vm_list_lock);
	list_del_init(&vm->list);
	write_unlock_bh(&acrn_vm_list_lock);

	put_vm(vm);
	filep->private_data = NULL;
 
	return 0;
}

static const struct file_operations fops = {
	.open = acrn_dev_open,
	.release = acrn_dev_release,
	.unlocked_ioctl = acrn_dev_ioctl,
};

static struct miscdevice acrn_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "acrn_hsm",
	.fops	= &fops,
};

#define SUPPORT_HV_API_VERSION_MAJOR	1
#define SUPPORT_HV_API_VERSION_MINOR	0
static int __init acrn_init(void)
{
	int ret;

	if (x86_hyper_type != X86_HYPER_ACRN)
		return -ENODEV;

	if (!acrn_is_privilege_vm())
		return -EPERM;

	memset(&acrn_api_version, 0, sizeof(acrn_api_version));
	if (hcall_get_api_version(slow_virt_to_phys(&acrn_api_version)) < 0) {
		pr_err("acrn: failed to get api version from Hypervisor !\n");
		return -EINVAL;
	}

	if (acrn_api_version.major_version >= SUPPORT_HV_API_VERSION_MAJOR &&
	    acrn_api_version.minor_version >= SUPPORT_HV_API_VERSION_MINOR) {
		pr_info("ACRN: hv api version %d.%d\n",
			acrn_api_version.major_version,
			acrn_api_version.minor_version);
	} else {
		pr_err("ACRN: not support hv api version %d.%d!\n",
		       acrn_api_version.major_version,
		       acrn_api_version.minor_version);
		return -EINVAL;
	}

	ret = misc_register(&acrn_dev);
	if (ret) {
		pr_err("Can't register acrn as misc dev\n");
		return ret;
	}

	return 0;
}

static void __exit acrn_exit(void)
{
	misc_deregister(&acrn_dev);
}

module_init(acrn_init);
module_exit(acrn_exit);

MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is a skeleton char device driver for ACRN\n");
MODULE_VERSION("0.1");
