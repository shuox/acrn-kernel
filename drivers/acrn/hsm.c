// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN hyperviosr service module (HSM)
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:	Shuo A Liu <shuo.a.liu@intel.com>
 * 		Yakui Zhao <yakui.zhao@intel.com>
 */

#include <linux/bits.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <asm/acrn.h>
#include <asm/hypervisor.h>
#include <linux/acrn.h>
#include <linux/acrn_host.h>
#include "acrn_drv.h"

#define SUPPORT_HV_API_VERSION_MAJOR	1
#define SUPPORT_HV_API_VERSION_MINOR	0
static struct api_version acrn_api_version;

static int acrn_dev_open(struct inode *inodep, struct file *filep)
{
	struct acrn_vm *vm;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	vm->vmid = ACRN_INVALID_VMID;
	filep->private_data = vm;
	return 0;
}

static long acrn_dev_ioctl(struct file *filep,
		    unsigned int cmd, unsigned long ioctl_param)
{
	struct acrn_vm *vm;
	struct acrn_create_vm *vm_param;
	struct vm_memmap memmap;
	struct acrn_set_vcpu_regs *cpu_regs;
	struct ic_ptdev_irq ic_pt_irq, *hc_pt_irq;
	struct acrn_msi_entry *msi;
	struct ioreq_notify notify;
	struct acrn_pcidev *pcidev;
	struct acrn_ioeventfd ioeventfd;
	struct acrn_irqfd irqfd;
	struct page *page;
	u64 cstate_cmd;
	int ret = 0;

	vm = (struct acrn_vm *)filep->private_data;
	if (cmd == IC_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param, &acrn_api_version,
				 sizeof(acrn_api_version)))
			return -EFAULT;
		return 0;
	}

	if ((vm->vmid == ACRN_INVALID_VMID) && (cmd != IC_CREATE_VM)) {
		pr_err("acrn: invalid VM ID for IOCTL %x!\n", cmd);
		return -EFAULT;
	}

	switch (cmd) {
	case IC_CREATE_VM:
		vm_param = kmalloc(sizeof(*vm_param), GFP_KERNEL);
		if (!vm_param)
			return -ENOMEM;

		if (copy_from_user(vm_param, (void __user *)ioctl_param,
				   sizeof(struct acrn_create_vm))) {
			ret = -EFAULT;
			goto err_create_vm;
		}

		vm = acrn_vm_create(vm, vm_param);
		if (!vm) {
			ret = -EFAULT;
			goto err_create_vm;
		}

		if (copy_to_user((void __user *)ioctl_param, vm_param,
				 sizeof(struct acrn_create_vm)))
			ret = -EFAULT;
err_create_vm:
		kfree(vm_param);
		break;
	case IC_START_VM:
		ret = hcall_start_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to start VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case IC_PAUSE_VM:
		ret = hcall_pause_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to pause VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case IC_RESET_VM:
		ret = hcall_reset_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to restart VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case IC_DESTROY_VM:
		ret = acrn_vm_destroy(vm);
		break;
	case IC_SET_VCPU_REGS:
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
			pr_err("acrn: Failed to set regs state of vm %d!\n",
			       vm->vmid);
			return -EFAULT;
		}
		break;
	case IC_SET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = map_guest_memseg(vm, &memmap);
		break;
	case IC_UNSET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = unmap_guest_memseg(vm, &memmap);
		break;
	case IC_ASSIGN_PCIDEV:
		pcidev = kmalloc(sizeof(struct acrn_pcidev), GFP_KERNEL);
		if (pcidev == NULL)
			return -ENOMEM;
		if (copy_from_user(pcidev,
				(void *)ioctl_param, sizeof(*pcidev)))
			ret = -EFAULT;

		ret = hcall_assign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			pr_err("acrn: Failed to assign pci device!\n");
		kfree(pcidev);
		break;
	case IC_DEASSIGN_PCIDEV:
		pcidev = kmalloc(sizeof(struct acrn_pcidev), GFP_KERNEL);
		if (pcidev == NULL)
			return -ENOMEM;
		if (copy_from_user(pcidev,
				(void *)ioctl_param, sizeof(*pcidev)))
			ret = -EFAULT;
		ret = hcall_deassign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			pr_err("acrn: Failed to deassign pci device!\n");
		kfree(pcidev);
		break;
	case IC_SET_PTDEV_INTR_INFO:
		if (copy_from_user(&ic_pt_irq, (void __user *)ioctl_param,
				   sizeof(ic_pt_irq)))
			return -EFAULT;

		hc_pt_irq = kmemdup(&ic_pt_irq, sizeof(ic_pt_irq), GFP_KERNEL);
		if (IS_ERR_OR_NULL(hc_pt_irq))
			return -ENOMEM;

		ret = hcall_set_ptdev_intr_info(vm->vmid,
						virt_to_phys(hc_pt_irq));
		kfree(hc_pt_irq);
		if (ret < 0) {
			pr_err("acrn: failed to set intr info for ptdev!\n");
			return -EFAULT;
		}
		break;
	case IC_RESET_PTDEV_INTR_INFO:
		if (copy_from_user(&ic_pt_irq, (void __user *)ioctl_param,
				   sizeof(ic_pt_irq)))
			return -EFAULT;

		hc_pt_irq = kmemdup(&ic_pt_irq, sizeof(ic_pt_irq), GFP_KERNEL);
		if (!hc_pt_irq)
			return -ENOMEM;

		ret = hcall_reset_ptdev_intr_info(vm->vmid,
						  virt_to_phys(hc_pt_irq));
		kfree(hc_pt_irq);
		if (ret < 0) {
			pr_err("acrn: failed to reset intr info for ptdev!\n");
			return -EFAULT;
		}
		break;
	case IC_SET_IRQLINE:
		ret = hcall_set_irqline(vm->vmid, ioctl_param);
		if (ret < 0) {
			pr_err("acrn: failed to set irqline!\n");
			return -EFAULT;
		}
		break;
	case IC_INJECT_MSI:
		msi = kmalloc(sizeof(*msi), GFP_KERNEL);
		if (!msi)
			return -ENOMEM;

		if (copy_from_user(msi, (void __user *)ioctl_param,
				   sizeof(*msi))) {
			kfree(msi);
			return -EFAULT;
		}

		ret = hcall_inject_msi(vm->vmid, virt_to_phys(msi));
		kfree(msi);
		if (ret < 0) {
			pr_err("acrn: failed to inject!\n");
			return -EFAULT;
		}
		break;
	case IC_VM_INTR_MONITOR:
		ret = get_user_pages_fast(ioctl_param, 1, 1, &page);
		if (unlikely(ret != 1)) {
			pr_err("acrn-dev: failed to pin intr hdr buffer!\n");
			return -ENOMEM;
		}

		ret = hcall_vm_intr_monitor(vm->vmid, page_to_phys(page));
		if (ret < 0) {
			put_page(page);
			pr_err("acrn-dev: monitor intr data err=%d\n", ret);
			return -EFAULT;
		}
		if (vm->monitor_page)
			put_page(vm->monitor_page);
		vm->monitor_page = page;
		break;
	case IC_CREATE_IOREQ_CLIENT:
		if (!acrn_ioreq_create_client(vm, NULL, NULL, true, "acrndm"))
			ret = -EFAULT;
		break;
	case IC_DESTROY_IOREQ_CLIENT:
		acrn_ioreq_destroy_client(get_default_client(vm));
		break;
	case IC_ATTACH_IOREQ_CLIENT:
		ret = acrn_ioreq_attach_client(get_default_client(vm));
		break;
	case IC_NOTIFY_REQUEST_FINISH:
		if (copy_from_user(&notify, (void __user *)ioctl_param,
				   sizeof(notify)))
			return -EFAULT;
		ret = acrn_ioreq_complete_request(get_default_client(vm),
						notify.vcpu, NULL);
		if (ret < 0)
			return -EFAULT;
		break;
	case IC_CLEAR_VM_IOREQ:
		acrn_ioreq_clear_request(vm);
		break;
	case IC_PM_GET_CPU_STATE:
		if (copy_from_user(&cstate_cmd,
				(void *)ioctl_param, sizeof(cstate_cmd)))
			return -EFAULT;

		switch (cstate_cmd & PMCMD_TYPE_MASK) {
		case PMCMD_GET_PX_CNT:
		case PMCMD_GET_CX_CNT: {
			u64 *pm_info;

			pm_info = kmalloc(sizeof(u64), GFP_KERNEL);
			if (!pm_info)
				return -ENOMEM;

			ret = hcall_get_cpu_state(cstate_cmd, virt_to_phys(pm_info));
			if (ret < 0) {
				kfree(pm_info);
				return -EFAULT;
			}

			if (copy_to_user((void __user *)ioctl_param,
					 pm_info, sizeof(u64)))
				ret = -EFAULT;

			kfree(pm_info);
			break;
		}
		case PMCMD_GET_PX_DATA: {
			struct cpu_px_data *px_data;

			px_data = kmalloc(sizeof(*px_data), GFP_KERNEL);
			if (!px_data)
				return -ENOMEM;

			ret = hcall_get_cpu_state(cstate_cmd, virt_to_phys(px_data));
			if (ret < 0) {
				kfree(px_data);
				return -EFAULT;
			}

			if (copy_to_user((void __user *)ioctl_param,
					 px_data, sizeof(*px_data)))
				ret = -EFAULT;

			kfree(px_data);
			break;
		}
		case PMCMD_GET_CX_DATA: {
			struct cpu_cx_data *cx_data;

			cx_data = kmalloc(sizeof(*cx_data), GFP_KERNEL);
			if (!cx_data)
				return -ENOMEM;

			ret = hcall_get_cpu_state(cstate_cmd, virt_to_phys(cx_data));
			if (ret < 0) {
				kfree(cx_data);
				return -EFAULT;
			}

			if (copy_to_user((void __user *)ioctl_param,
					 cx_data, sizeof(*cx_data)))
				ret = -EFAULT;
			kfree(cx_data);
			break;
		}
		default:
			ret = -EFAULT;
		}
		break;
	case IC_EVENT_IOEVENTFD:
		if (copy_from_user(&ioeventfd, (void __user *)ioctl_param,
				   sizeof(ioeventfd)))
			return -EFAULT;

		ret = acrn_ioeventfd_config(vm, &ioeventfd);
		break;
	case IC_EVENT_IRQFD:
		if (copy_from_user(&irqfd, (void __user *)ioctl_param,
				   sizeof(irqfd)))
			return -EFAULT;
		ret = acrn_irqfd_config(vm, &irqfd);
		break;
	default:
		pr_warn("acrn: Unknown IOCTL 0x%x!\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static int acrn_dev_release(struct inode *inodep, struct file *filep)
{
	struct acrn_vm *vm = filep->private_data;

	acrn_vm_destroy(vm);
	kfree(vm);
	return 0;
}

static const struct file_operations acrn_fops = {
	.open = acrn_dev_open,
	.release = acrn_dev_release,
	.unlocked_ioctl = acrn_dev_ioctl,
};

static struct miscdevice acrn_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "acrn_hsm",
	.fops	= &acrn_fops,
};

static ssize_t offline_cpu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u64 cpu, lapicid;
	int ret;

	ret = kstrtoull(buf, 0, &cpu);
	if (ret)
		return ret;

	if (cpu_possible(cpu)) {
		lapicid = cpu_data(cpu).apicid;
		pr_info("acrn: Try to offline cpu %lld with lapicid %lld\n",
				cpu, lapicid);
		if (hcall_sos_offline_cpu(lapicid) < 0) {
			pr_err("acrn: Failed to offline cpu from Hypervisor!\n");
			return -EINVAL;
		}
	}
	return count;
}

static DEVICE_ATTR(offline_cpu, 00200, NULL, offline_cpu_store);

static struct attribute *acrn_attrs[] = {
	&dev_attr_offline_cpu.attr,
	NULL
};

static struct attribute_group acrn_attr_group = {
	.attrs = acrn_attrs,
};

static int __init hsm_init(void)
{
	int ret;

	if (x86_hyper_type != X86_HYPER_ACRN)
		return -ENODEV;

	if (!acrn_is_privileged_vm())
		return -EPERM;

	memset(&acrn_api_version, 0, sizeof(acrn_api_version));
	if (hcall_get_api_version(slow_virt_to_phys(&acrn_api_version)) < 0) {
		pr_err("acrn: Failed to get API version from hypervisor!\n");
		return -EINVAL;
	}

	if (acrn_api_version.major_version >= SUPPORT_HV_API_VERSION_MAJOR &&
	    acrn_api_version.minor_version >= SUPPORT_HV_API_VERSION_MINOR) {
		pr_info("acrn: API version is %d.%d\n",
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
		pr_err("acrn: Create misc dev failed!\n");
		return ret;
	}

	if (sysfs_create_group(&acrn_dev.this_device->kobj, &acrn_attr_group)) {
		pr_warn("acrn: sysfs create failed\n");
		misc_deregister(&acrn_dev);
		return -EINVAL;
	}
	acrn_setup_ioreq_intr();

	return 0;
}

static void __exit hsm_exit(void)
{
	sysfs_remove_group(&acrn_dev.this_device->kobj, &acrn_attr_group);
	misc_deregister(&acrn_dev);
	acrn_remove_intr_irq();
}

module_init(hsm_init);
module_exit(hsm_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("ACRN hyperviosr service module(HSM)");
