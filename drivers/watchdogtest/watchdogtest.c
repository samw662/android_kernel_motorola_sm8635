// SPDX-License-Identifier: GPL-2.0-only
/*
 * Simple softlockup watchdog regression test module
 *
 * Copyright (C) 2012 Motorola Mobility LLC.
 */

#include <linux/capability.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/preempt.h>
#include <soc/qcom/watchdog.h>

static struct dentry *fire_softlockup;
static struct dentry *fire_watchdog;

static int fire_watchdog_reset_set(void *data, u64 value)
{
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	if (value != 1)
		return -EINVAL;

	pr_crit("Motorola watchdog test: triggering hardware reset\n");
	return qcom_wdt_test_trigger_bite();
}
DEFINE_SIMPLE_ATTRIBUTE(fire_watchdog_reset_fops, NULL,
			fire_watchdog_reset_set, "%llu\n");

static int fire_softlockup_reset_set(void *data, u64 value)
{
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;
	if (value != 1)
		return -EINVAL;

	pr_crit("Motorola watchdog test: triggering soft lockup\n");
	preempt_disable();
	for (;;)
		cpu_relax();
}
DEFINE_SIMPLE_ATTRIBUTE(fire_softlockup_reset_fops, NULL,
			fire_softlockup_reset_set, "%llu\n");

static int __init watchdog_test_init(void)
{
	fire_watchdog = debugfs_create_file("fire_watchdog_reset", 0200,
					   NULL, NULL, &fire_watchdog_reset_fops);
	if (IS_ERR(fire_watchdog))
		return PTR_ERR(fire_watchdog);

	if (IS_ENABLED(CONFIG_SOFTLOCKUP_DETECTOR)) {
		fire_softlockup = debugfs_create_file("fire_softlockup_reset", 0200,
						     NULL, NULL, &fire_softlockup_reset_fops);
		if (IS_ERR(fire_softlockup)) {
			debugfs_remove(fire_watchdog);
			return PTR_ERR(fire_softlockup);
		}
	}
	return 0;
}

static void __exit watchdog_test_exit(void)
{
	debugfs_remove(fire_softlockup);
	debugfs_remove(fire_watchdog);
}

module_init(watchdog_test_init);
module_exit(watchdog_test_exit);
MODULE_LICENSE("GPL");
