/*
 * Copyright (C) 2023 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "moto_sched: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <linux/sched/cputime.h>
#endif
#include <kernel/sched/sched.h>

#include "msched_sysfs.h"
#include "msched_common.h"
#include "mdpf/mdpf_sysfs.h"

#define MOTO_OEM_DATA_SIZE_TEST(wstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(wstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_oem_data1)))


extern int locking_opt_init(void);
extern void locking_opt_exit(void);

static int __init moto_sched_init(void)
{
	int ret = 0;

	/* compile time checks for oem data size */
	MOTO_OEM_DATA_SIZE_TEST(struct moto_task_struct, struct task_struct);

	ret = moto_sched_proc_init();
	if (ret != 0)
		return ret;
	ret = mdpf_proc_init();
	if (ret != 0) {
		pr_err("mdpf_proc_init failed!\n");
		goto err_proc;
	}

	ret = msched_binder_register();
	if (ret)
		goto err_mdpf;
	ret = locking_opt_init();
	if (ret)
		goto err_binder;
	ret = moto_sched_register_ops();
	if (ret)
		goto err_locking;
	ret = register_vendor_comm_hooks();
	if (ret)
		goto err_ops;
	moto_sched_enable_writes();

	pr_info("moto_sched_init succeed!\n");
	return 0;

err_ops:
	set_moto_sched_ops(NULL);
err_locking:
	locking_opt_exit();
err_binder:
	msched_binder_unregister();
err_mdpf:
	mdpf_proc_deinit();
err_proc:
	moto_sched_proc_deinit();
	return ret;
}

module_init(moto_sched_init);
MODULE_DESCRIPTION("Motorola CPU sched optimizations driver");
MODULE_LICENSE("GPL v2");
