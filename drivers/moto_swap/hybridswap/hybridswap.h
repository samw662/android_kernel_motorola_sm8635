// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */

#ifndef HYBRIDSWAP_H
#define HYBRIDSWAP_H
#include <linux/wait_bit.h>
extern int __init hybridswap_pre_init(void);
void hybridswap_exit(void);
extern ssize_t hybridswap_vmstat_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_loglevel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);
#ifdef CONFIG_HYBRIDSWAP_CORE
extern void hybridswap_record(struct zram *zram, u32 index, struct mem_cgroup *memcg);
extern void hybridswap_untrack(struct zram *zram, u32 index);
extern int hybridswap_page_fault(struct zram *zram, u32 index);
bool hybridswap_owns_zram(struct zram *zram);
void hybridswap_wake_reclaim(void);
void hybridswap_stop_swapd(struct zram *zram);
void hybridswap_quiesce(struct zram *zram);
void hybridswap_release(struct zram *zram);
static inline bool hybridswap_slot_busy(struct zram *zram, u32 index)
{
	return READ_ONCE(zram->table[index].flags) &
	       (BIT(ZRAM_HYB_WRITE) | BIT(ZRAM_BATCHING_OUT));
}

static inline void hybridswap_slot_clear_busy(struct zram *zram, u32 index,
					     enum zram_pageflags flag)
{
	zram_clear_flag(zram, index, flag);
	wake_up_var(&zram->table[index].flags);
}

extern ssize_t hybridswap_report_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_stat_snap_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_meminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_core_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_core_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_loop_device_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_loop_device_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_dev_life_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_dev_life_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_quota_day_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_quota_day_show(struct device *dev,
		struct device_attribute *attr, char *buf);
extern ssize_t hybridswap_zram_increase_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_zram_increase_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static inline bool skip_zram_write(struct zram *zram, u32 index)
{
	zram_slot_lock(zram, index);
	if (zram_test_flag(zram, index, ZRAM_HYB_WRITE) || zram_test_flag(zram, index, ZRAM_BATCHING_OUT)) {
		zram_slot_unlock(zram, index);
		pr_info("zram is under wb or batching out index=%d\n", index);
		return true;
	}
	zram_slot_unlock(zram, index);
	return false;
}
#endif

#ifdef CONFIG_HYBRIDSWAP_SWAPD
extern ssize_t hybridswap_swapd_pause_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len);
extern ssize_t hybridswap_swapd_pause_show(struct device *dev,
		struct device_attribute *attr, char *buf);
#endif
static inline bool current_is_mswapd(void)
{
#ifdef CONFIG_HYBRIDSWAP_SWAPD
	return (strncmp(current->comm, "mswapd:", sizeof("mswapd:") - 1) == 0);
#else
	return false;
#endif
}
#endif /* HYBRIDSWAP_H */
