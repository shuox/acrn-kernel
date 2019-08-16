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
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <asm/acrn.h>
#include <asm/hypervisor.h>

#define	DEVICE_NAME	"acrn_hsm"

static
int acrn_dev_open(struct inode *inodep, struct file *filep)
{
	return 0;
}

static
long acrn_dev_ioctl(struct file *filep,
		    unsigned int ioctl_num, unsigned long ioctl_param)
{
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

static int __init acrn_init(void)
{
	int ret;

	if (x86_hyper_type != X86_HYPER_ACRN)
		return -ENODEV;

	if (!acrn_is_privilege_vm())
		return -EPERM;

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
