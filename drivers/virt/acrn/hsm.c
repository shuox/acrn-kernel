// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * ACRN Hyperviosr Service Module (HSM)
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * Authors:
 *	Fengwei Yin <fengwei.yin@intel.com>
 *	Yakui Zhao <yakui.zhao@intel.com>
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <asm/hypervisor.h>
#include <asm/acrn.h>

#include "acrn_drv.h"

static struct acrn_api_version api_version;

static int acrn_dev_open(struct inode *inode, struct file *filp)
{
	struct acrn_vm *vm;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	vm->vmid = ACRN_INVALID_VMID;
	filp->private_data = vm;
	return 0;
}

static int pmcmd_ioctl(u64 cmd, void __user *uptr)
{
	int ret = 0;
	u64 *pm_info;
	struct acrn_pstate_data *px_data;
	struct acrn_cstate_data *cx_data;

	switch (cmd & PMCMD_TYPE_MASK) {
	case ACRN_PMCMD_GET_PX_CNT:
	case ACRN_PMCMD_GET_CX_CNT:
		pm_info = kmalloc(sizeof(u64), GFP_KERNEL);
		if (!pm_info)
			return -ENOMEM;

		ret = hcall_get_cpu_state(cmd, virt_to_phys(pm_info));
		if (ret < 0) {
			kfree(pm_info);
			break;
		}

		if (copy_to_user(uptr, pm_info, sizeof(u64)))
			ret = -EFAULT;
		kfree(pm_info);
		break;
	case ACRN_PMCMD_GET_PX_DATA:
		px_data = kmalloc(sizeof(*px_data), GFP_KERNEL);
		if (!px_data)
			return -ENOMEM;

		ret = hcall_get_cpu_state(cmd, virt_to_phys(px_data));
		if (ret < 0) {
			kfree(px_data);
			break;
		}

		if (copy_to_user(uptr, px_data, sizeof(*px_data)))
			ret = -EFAULT;
		kfree(px_data);
		break;
	case ACRN_PMCMD_GET_CX_DATA:
		cx_data = kmalloc(sizeof(*cx_data), GFP_KERNEL);
		if (!cx_data)
			return -ENOMEM;

		ret = hcall_get_cpu_state(cmd, virt_to_phys(cx_data));
		if (ret < 0) {
			kfree(cx_data);
			break;
		}

		if (copy_to_user(uptr, cx_data, sizeof(*cx_data)))
			ret = -EFAULT;
		kfree(cx_data);
		break;
	default:
		break;
	}

	return ret;
}

static long acrn_dev_ioctl(struct file *filp,
		    unsigned int cmd, unsigned long ioctl_param)
{
	struct acrn_vm *vm;
	struct acrn_create_vm *vm_param;
	struct acrn_vm_memmap memmap;
	struct acrn_set_vcpu_regs *cpu_regs;
	struct acrn_ioreq_notify notify;
	struct acrn_ptdev_irq *irq_info;
	struct acrn_msi_entry *msi;
	struct acrn_pcidev *pcidev;
	struct acrn_ioeventfd ioeventfd;
	struct acrn_irqfd irqfd;
	struct page *page;
	u64 cstate_cmd;
	int ret = 0;

	vm = (struct acrn_vm *)filp->private_data;
	if (cmd == ACRN_IOCTL_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param,
					&api_version, sizeof(api_version)))
			return -EFAULT;
		return 0;
	}

	if ((vm->vmid == ACRN_INVALID_VMID) && (cmd != ACRN_IOCTL_CREATE_VM)) {
		pr_err("acrn: ioctl 0x%x: Invalid VM state!\n", cmd);
		return -EFAULT;
	}

	switch (cmd) {
	case ACRN_IOCTL_CREATE_VM:
		vm_param = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_create_vm));
		if (IS_ERR(vm_param))
			return PTR_ERR(vm_param);

		vm = acrn_vm_create(vm, vm_param);
		if (!vm) {
			ret = -EFAULT;
			kfree(vm_param);
			break;
		}

		if (copy_to_user((void __user *)ioctl_param, vm_param,
				 sizeof(struct acrn_create_vm)))
			ret = -EFAULT;

		kfree(vm_param);
		break;
	case ACRN_IOCTL_START_VM:
		ret = hcall_start_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to start VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case ACRN_IOCTL_PAUSE_VM:
		ret = hcall_pause_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to pause VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case ACRN_IOCTL_RESET_VM:
		ret = hcall_reset_vm(vm->vmid);
		if (ret < 0) {
			pr_err("acrn: Failed to restart VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case ACRN_IOCTL_DESTROY_VM:
		ret = acrn_vm_destroy(vm);
		if (ret < 0) {
			pr_err("acrn: Failed to destroy VM %d!\n", vm->vmid);
			ret = -EFAULT;
		}
		break;
	case ACRN_IOCTL_SET_VCPU_REGS:
		cpu_regs = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_set_vcpu_regs));
		if (IS_ERR(cpu_regs))
			return PTR_ERR(cpu_regs);

		ret = hcall_set_vcpu_regs(vm->vmid, virt_to_phys(cpu_regs));
		kfree(cpu_regs);
		if (ret < 0)
			pr_err("acrn: Failed to set regs state of VM %d!\n",
			       vm->vmid);
		break;
	case ACRN_IOCTL_SET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = acrn_map_guest_memseg(vm, &memmap);
		break;
	case ACRN_IOCTL_UNSET_MEMSEG:
		if (copy_from_user(&memmap, (void __user *)ioctl_param,
				   sizeof(memmap)))
			return -EFAULT;

		ret = acrn_unmap_guest_memseg(vm, &memmap);
		break;
	case ACRN_IOCTL_ASSIGN_PCIDEV:
		pcidev = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_pcidev));
		if (IS_ERR(pcidev))
			return PTR_ERR(pcidev);

		ret = hcall_assign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			pr_err("acrn: Failed to assign pci device!\n");
		kfree(pcidev);
		break;
	case ACRN_IOCTL_DEASSIGN_PCIDEV:
		pcidev = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_pcidev));
		if (IS_ERR(pcidev))
			return PTR_ERR(pcidev);

		ret = hcall_deassign_pcidev(vm->vmid, virt_to_phys(pcidev));
		if (ret < 0)
			pr_err("acrn: Failed to deassign pci device!\n");
		kfree(pcidev);
		break;
	case ACRN_IOCTL_SET_PTDEV_INTR:
		irq_info = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_ptdev_irq));
		if (IS_ERR(irq_info))
			return PTR_ERR(irq_info);

		ret = hcall_set_ptdev_intr(vm->vmid, virt_to_phys(irq_info));
		kfree(irq_info);
		if (ret < 0)
			pr_err("acrn: Failed to configure intr for ptdev!\n");
		break;
	case ACRN_IOCTL_RESET_PTDEV_INTR:
		irq_info = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_ptdev_irq));
		if (IS_ERR(irq_info))
			return PTR_ERR(irq_info);

		ret = hcall_reset_ptdev_intr(vm->vmid, virt_to_phys(irq_info));
		kfree(irq_info);
		if (ret < 0)
			pr_err("acrn: Failed to reset intr for ptdev!\n");
		break;
	case ACRN_IOCTL_SET_IRQLINE:
		ret = hcall_set_irqline(vm->vmid, ioctl_param);
		if (ret < 0)
			pr_err("acrn: Failed to set interrupt line!\n");
		break;
	case ACRN_IOCTL_INJECT_MSI:
		msi = memdup_user((void __user *)ioctl_param,
				sizeof(struct acrn_msi_entry));
		if (IS_ERR(msi))
			return PTR_ERR(msi);

		ret = hcall_inject_msi(vm->vmid, virt_to_phys(msi));
		kfree(msi);
		if (ret < 0)
			pr_err("acrn: Failed to inject MSI!\n");
		break;
	case ACRN_IOCTL_VM_INTR_MONITOR:
		ret = get_user_pages_fast(ioctl_param, 1, 1, &page);
		if (unlikely(ret != 1)) {
			pr_err("acrn-dev: Failed to pin intr hdr buffer!\n");
			return -ENOMEM;
		}

		ret = hcall_vm_intr_monitor(vm->vmid, page_to_phys(page));
		if (ret < 0) {
			put_page(page);
			pr_err("acrn-dev: Failed to monitor intr data!\n");
		}
		if (vm->monitor_page)
			put_page(vm->monitor_page);
		vm->monitor_page = page;
		break;
	case ACRN_IOCTL_CREATE_IOREQ_CLIENT:
		if (!acrn_ioreq_create_client(vm, NULL, NULL, true, "acrndm"))
			ret = -EFAULT;
		break;
	case ACRN_IOCTL_DESTROY_IOREQ_CLIENT:
		if (vm->default_client)
			acrn_ioreq_destroy_client(vm->default_client);
		break;
	case ACRN_IOCTL_ATTACH_IOREQ_CLIENT:
		if (vm->default_client)
			ret = acrn_ioreq_wait_client(vm->default_client);
		break;
	case ACRN_IOCTL_NOTIFY_REQUEST_FINISH:
		if (copy_from_user(&notify, (void __user *)ioctl_param,
				   sizeof(notify)))
			return -EFAULT;
		ret = acrn_ioreq_complete_request_default(vm, notify.vcpu);
		if (ret < 0)
			return -EFAULT;
		break;
	case ACRN_IOCTL_CLEAR_VM_IOREQ:
		acrn_ioreq_clear_request(vm);
		break;
	case ACRN_IOCTL_PM_GET_CPU_STATE:
		if (copy_from_user(&cstate_cmd,
				(void *)ioctl_param, sizeof(cstate_cmd)))
			return -EFAULT;

		ret = pmcmd_ioctl(cstate_cmd, (void __user *)ioctl_param);
		break;
	case ACRN_IOCTL_IOEVENTFD:
		if (copy_from_user(&ioeventfd, (void __user *)ioctl_param,
				   sizeof(ioeventfd)))
			return -EFAULT;

		ret = acrn_ioeventfd_config(vm, &ioeventfd);
		break;
	case ACRN_IOCTL_IRQFD:
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

static int acrn_dev_release(struct inode *inode, struct file *filp)
{
	struct acrn_vm *vm = filp->private_data;

	acrn_vm_destroy(vm);
	kfree(vm);
	return 0;
}

static const struct file_operations acrn_fops = {
	.owner = THIS_MODULE,
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

	if (kstrtoull(buf, 0, &cpu) < 0)
		return -EINVAL;

	if (cpu >= num_possible_cpus() || cpu == 0 || !cpu_is_hotpluggable(cpu))
		return -EINVAL;

	if (cpu_online(cpu))
		remove_cpu(cpu);

	lapicid = cpu_data(cpu).apicid;
	pr_debug("acrn: Try to offline cpu %lld with lapicid %lld\n",
			cpu, lapicid);
	if (hcall_sos_offline_cpu(lapicid) < 0) {
		pr_err("acrn: Failed to offline cpu from Hypervisor!\n");
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_WO(offline_cpu);

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

	if (hcall_get_api_version(slow_virt_to_phys(&api_version)) < 0) {
		pr_err("acrn: Failed to get API version from hypervisor!\n");
		return -EINVAL;
	}

	pr_info("acrn: API version is %d.%d\n",
			api_version.major_version, api_version.minor_version);

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
MODULE_DESCRIPTION("ACRN Hypervisor Service Module (HSM)");
