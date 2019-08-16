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

#define	DEVICE_NAME	"acrn_hsm"

static struct api_version acrn_api_version;

static
int acrn_dev_open(struct inode *inodep, struct file *filep)
{
	return 0;
}

static
long acrn_dev_ioctl(struct file *filep,
		    unsigned int ioctl_num, unsigned long ioctl_param)
{
	if (ioctl_num == IC_GET_API_VERSION) {
		if (copy_to_user((void __user *)ioctl_param, &acrn_api_version,
				 sizeof(acrn_api_version)))
			return -EFAULT;
	}

	return 0;
}

static int acrn_dev_release(struct inode *inodep, struct file *filep)
{
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
