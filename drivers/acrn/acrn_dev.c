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
#include <linux/acrn/acrn_ioctl_defs.h>
#include <linux/acrn/acrn_drv.h>

#include "acrn_hypercall.h"
#include "acrn_drv_internal.h"

#define	DEVICE_NAME	"acrn_hsm"

static struct api_version acrn_api_version;
static struct tasklet_struct acrn_io_req_tasklet;

static
int acrn_dev_open(struct inode *inodep, struct file *filep)
{
	struct acrn_vm *vm;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return -ENOMEM;

	refcount_set(&vm->refcnt, 1);
	vm->vmid = ACRN_INVALID_VMID;

	mutex_init(&vm->regions_mapping_lock);

	INIT_LIST_HEAD(&vm->ioreq_client_list);
	spin_lock_init(&vm->ioreq_client_lock);

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
	int ret = 0;

	vm = (struct acrn_vm *)filep->private_data;
	if (!vm) {
		pr_err("acrn: invalid VM !\n");
		return -EFAULT;
	}
 
	if (ioctl_num == IC_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param, &acrn_api_version,
				 sizeof(acrn_api_version)))
			return -EFAULT;
		return 0;
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

		ret = acrn_ioreq_init(vm, created_vm->req_buf);
		if (ret < 0)
			goto ioreq_buf_fail;

		acrn_ioeventfd_init(vm->vmid);
		acrn_irqfd_init(vm->vmid);
		pr_debug("acrn: VM %d created\n", created_vm->vmid);
		kfree(created_vm);
		break;

ioreq_buf_fail:
		hcall_destroy_vm(created_vm->vmid);
		vm->vmid = ACRN_INVALID_VMID;
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

	case IC_ASSIGN_PTDEV: {
		unsigned short bdf;

		if (copy_from_user(&bdf, (void __user *)ioctl_param,
				   sizeof(unsigned short)))
			return -EFAULT;

		ret = hcall_assign_ptdev(vm->vmid, bdf);
		if (ret < 0) {
			pr_err("acrn: failed to assign ptdev!\n");
			return -EFAULT;
		}
		break;
	}
	case IC_DEASSIGN_PTDEV: {
		unsigned short bdf;

		if (copy_from_user(&bdf, (void __user *)ioctl_param,
				   sizeof(unsigned short)))
			return -EFAULT;

		ret = hcall_deassign_ptdev(vm->vmid, bdf);
		if (ret < 0) {
			pr_err("acrn: failed to deassign ptdev!\n");
			return -EFAULT;
		}
		break;
	}

	case IC_SET_PTDEV_INTR_INFO: {
		struct ic_ptdev_irq ic_pt_irq, *hc_pt_irq;

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
	}
	case IC_RESET_PTDEV_INTR_INFO: {
		struct ic_ptdev_irq ic_pt_irq, *hc_pt_irq;

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
	}

	case IC_SET_IRQLINE: {
		ret = hcall_set_irqline(vm->vmid, ioctl_param);
		if (ret < 0) {
			pr_err("acrn: failed to set irqline!\n");
			return -EFAULT;
		}
		break;
	}

	case IC_INJECT_MSI: {
		struct acrn_msi_entry *msi;

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
	}

	case IC_VM_INTR_MONITOR: {
		struct page *page;

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
	}

	case IC_CREATE_IOREQ_CLIENT: {
		int client_id;

		client_id = acrn_ioreq_create_fallback_client(vm->vmid,
							      "acrndm");
		if (client_id < 0)
			return -EFAULT;
		return client_id;
	}

	case IC_DESTROY_IOREQ_CLIENT: {
		int client = ioctl_param;

		acrn_ioreq_destroy_client(client);
		break;
	}

	case IC_ATTACH_IOREQ_CLIENT: {
		int client = ioctl_param;

		return acrn_ioreq_attach_client(client);
	}

	case IC_NOTIFY_REQUEST_FINISH: {
		struct ioreq_notify notify;

		if (copy_from_user(&notify, (void __user *)ioctl_param,
				   sizeof(notify)))
			return -EFAULT;

		ret = acrn_ioreq_complete_request(notify.client_id,
						  notify.vcpu, NULL);
		if (ret < 0)
			return -EFAULT;
		break;
	}
	case IC_CLEAR_VM_IOREQ: {
		/*
		 * we need to flush the current pending ioreq dispatch
		 * tasklet and finish it before clearing all ioreq of this VM.
		 * With tasklet_kill, there still be a very rare race which
		 * might lost one ioreq tasklet for other VMs. So arm one after
		 * the clearing. It's harmless.
		 */
		tasklet_schedule(&acrn_io_req_tasklet);
		tasklet_kill(&acrn_io_req_tasklet);
		tasklet_schedule(&acrn_io_req_tasklet);
		acrn_ioreq_clear_request(vm);
		break;
	}

	case IC_PM_GET_CPU_STATE: {
		u64 cmd;

		if (copy_from_user(&cmd, (void *)ioctl_param, sizeof(cmd)))
			return -EFAULT;

		switch (cmd & PMCMD_TYPE_MASK) {
		case PMCMD_GET_PX_CNT:
		case PMCMD_GET_CX_CNT: {
			u64 *pm_info;

			pm_info = kmalloc(sizeof(u64), GFP_KERNEL);
			if (!pm_info)
				return -ENOMEM;

			ret = hcall_get_cpu_state(cmd, virt_to_phys(pm_info));
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

			ret = hcall_get_cpu_state(cmd, virt_to_phys(px_data));
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

			ret = hcall_get_cpu_state(cmd, virt_to_phys(cx_data));
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
			break;
		}

		break;
	}
	case IC_EVENT_IOEVENTFD: {
		struct acrn_ioeventfd args;

		if (copy_from_user(&args, (void __user *)ioctl_param,
				   sizeof(args)))
			return -EFAULT;

		ret = acrn_ioeventfd_config(vm->vmid, &args);
		break;
	}

	case IC_EVENT_IRQFD: {
		struct acrn_irqfd args;

		if (copy_from_user(&args, (void __user *)ioctl_param,
				   sizeof(args)))
			return -EFAULT;
		ret = acrn_irqfd_config(vm->vmid, &args);
		break;
	}

	default:
		pr_warn("Unknown IOCTL 0x%x\n", ioctl_num);
		ret = -EINVAL;
		break;
	}


	return ret;
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

static void io_req_tasklet(unsigned long data)
{
	struct acrn_vm *vm;
	/* This is already in tasklet. Use read_lock for list_lock */

	read_lock(&acrn_vm_list_lock);
	list_for_each_entry(vm, &acrn_vm_list, list) {
		if (!vm || !vm->req_buf)
			break;

		get_vm(vm);
		acrn_ioreq_distribute_request(vm);
		put_vm(vm);
	}
	read_unlock(&acrn_vm_list_lock);
}

static void acrn_intr_handler(void)
{
	tasklet_schedule(&acrn_io_req_tasklet);
}

static const struct file_operations fops = {
	.open = acrn_dev_open,
	.release = acrn_dev_release,
	.unlocked_ioctl = acrn_dev_ioctl,
	.poll = acrn_dev_poll,
};

static struct miscdevice acrn_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "acrn_hsm",
	.fops	= &fops,
};

#define SUPPORT_HV_API_VERSION_MAJOR	1
#define SUPPORT_HV_API_VERSION_MINOR	0

static ssize_t
offline_cpu_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	u64 cpu, lapicid;
	int ret;

	ret = kstrtoull(buf, 0, &cpu);
	if (ret)
		return ret;

	if ((cpu < num_possible_cpus()) && cpu_possible(cpu)) {
		lapicid = cpu_data(cpu).apicid;
		pr_info("acrn: try to offline cpu %lld with lapicid %lld\n",
				cpu, lapicid);
		if (hcall_sos_offline_cpu(lapicid) < 0) {
			pr_err("acrn: failed to offline cpu from Hypervisor!\n");
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

static int __init acrn_init(void)
{
	int ret;
	unsigned long flag;

	if (x86_hyper_type != X86_HYPER_ACRN)
		return -ENODEV;

	if (!acrn_is_privileged_vm())
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

	if (sysfs_create_group(&acrn_dev.this_device->kobj, &acrn_attr_group)) {
		pr_warn("acrn: sysfs create failed\n");
		misc_deregister(&acrn_dev);
		return -EINVAL;
	}

	tasklet_init(&acrn_io_req_tasklet, io_req_tasklet, 0);
	local_irq_save(flag);
	acrn_setup_intr_irq(acrn_intr_handler);
	local_irq_restore(flag);
	acrn_ioreq_driver_init();

	return 0;
}

static void __exit acrn_exit(void)
{
	tasklet_kill(&acrn_io_req_tasklet);
	acrn_remove_intr_irq();
	sysfs_remove_group(&acrn_dev.this_device->kobj, &acrn_attr_group);
	misc_deregister(&acrn_dev);
}

module_init(acrn_init);
module_exit(acrn_exit);

MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is a skeleton char device driver for ACRN\n");
MODULE_VERSION("0.1");
