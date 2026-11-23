// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Moto. All rights reserved.
 */

#define pr_fmt(fmt) "moto_locking_boost:" fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/tracepoint.h>

#include "locking_main.h"

int locking_opt_init(void)
{
	int ret = 0;

#ifdef CONFIG_MOTO_MUTEX_INHERIT
	ret = register_mutex_vendor_hooks();
	if (ret)
		return ret;
#endif

#ifdef CONFIG_MOTO_RWSEM_INHERIT
	ret = register_rwsem_vendor_hooks();
	if (ret)
		goto err_mutex;
#endif

#ifdef CONFIG_MOTO_FUTEX_INHERIT
	ret = register_futex_vendor_hooks();
	if (ret)
		goto err_rwsem;
#endif

	return ret;

#ifdef CONFIG_MOTO_FUTEX_INHERIT
err_rwsem:
#ifdef CONFIG_MOTO_RWSEM_INHERIT
	unregister_rwsem_vendor_hooks();
#endif
#endif
#if defined(CONFIG_MOTO_RWSEM_INHERIT) || defined(CONFIG_MOTO_FUTEX_INHERIT)
#ifdef CONFIG_MOTO_RWSEM_INHERIT
err_mutex:
#endif
#ifdef CONFIG_MOTO_MUTEX_INHERIT
	unregister_mutex_vendor_hooks();
#endif
	tracepoint_synchronize_unregister();
	return ret;
#endif
}

void locking_opt_exit(void)
{
#ifdef CONFIG_MOTO_FUTEX_INHERIT
	unregister_futex_vendor_hooks();
#endif

#ifdef CONFIG_MOTO_RWSEM_INHERIT
	unregister_rwsem_vendor_hooks();
#endif

#ifdef CONFIG_MOTO_MUTEX_INHERIT
	unregister_mutex_vendor_hooks();
#endif
	tracepoint_synchronize_unregister();

}
