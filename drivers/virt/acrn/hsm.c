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

static long acrn_dev_ioctl(struct file *filp,
		    unsigned int cmd, unsigned long ioctl_param)
{
	if (cmd == ACRN_IOCTL_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param,
					&api_version, sizeof(api_version)))
			return -EFAULT;
	}

	return 0;
}

static int acrn_dev_release(struct inode *inode, struct file *filp)
{
	struct acrn_vm *vm = filp->private_data;

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

	return 0;
}

static void __exit hsm_exit(void)
{
	misc_deregister(&acrn_dev);
}
module_init(hsm_init);
module_exit(hsm_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("ACRN Hypervisor Service Module (HSM)");
