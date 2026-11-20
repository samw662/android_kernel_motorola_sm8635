/*
 * Copyright (c) 2025 Motorola Inc.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <linux/sched/cputime.h>
#endif
#include <trace/hooks/sched.h>
#include <trace/hooks/binder.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/autogroup.h>
#include <drivers/android/binder_internal.h>

#include "msched_uclamp.h"
#include "msched_common.h"
#include "msched_sysfs.h"
#include "msched_trace.h"

#define MOTO_CLAMP_ACTIVE BIT(31)

static u32 msched_pack_clamp(const u16 clamp[UCLAMP_CNT])
{
	return MOTO_CLAMP_ACTIVE | ((u32)clamp[UCLAMP_MAX] << 16) |
	       clamp[UCLAMP_MIN];
}

static unsigned int msched_clamp_min(u32 clamp)
{
	return (u16)clamp;
}

static unsigned int msched_clamp_max(u32 clamp)
{
	return clamp ? (clamp & ~MOTO_CLAMP_ACTIVE) >> 16 : SCHED_CAPACITY_SCALE;
}

static void rvh_uclamp_eff_get(void *data, struct task_struct *p,
			       enum uclamp_id id, struct uclamp_se *limit,
			       struct uclamp_se *effective, int *ret)
{
	struct moto_task_struct *mts = get_moto_task_struct(p);
	struct uclamp_se req = p->uclamp_req[id];
	u32 binder, pi;
	unsigned int tg_min, tg_max, value;
	struct task_group *tg;

	if (*ret || !is_enabled(UX_ENABLE_MDPF))
		return;
	binder = READ_ONCE(mts->binder_clamp);
	pi = READ_ONCE(mts->pi_clamp);
	if (!binder && !pi)
		return;

	rcu_read_lock();
	tg = task_group(p);
	tg_min = task_group_is_autogroup(tg) || tg == &root_task_group ?
		 0 : tg->uclamp[UCLAMP_MIN].value;
	tg_max = tg->uclamp[UCLAMP_MAX].value;
	rcu_read_unlock();

	value = req.value;
	value = clamp(value, tg_min, tg_max);
	value = clamp(value, msched_clamp_min(pi), msched_clamp_max(pi));
	value = clamp(value, msched_clamp_min(binder), msched_clamp_max(binder));
	req.value = value;
	req.bucket_id = get_bucket_id(value);
	*effective = value > limit->value ? *limit : req;
	*ret = 1;

	trace_msched_uclamp_restriction_result(p->pid, id, p->uclamp_req[id].value,
		effective->value, limit->value, tg_min, tg_max,
		msched_clamp_min(pi), msched_clamp_max(pi),
		msched_clamp_min(binder), msched_clamp_max(binder));
}

static void msched_inherit_clamp(struct task_struct *p, struct task_struct *donor,
				u16 clamp[UCLAMP_CNT], unsigned int type)
{
	unsigned long p_util = moto_task_util(p), donor_util = moto_task_util(donor);
	unsigned int p_min = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned int p_max = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned int min = uclamp_eff_value(donor, UCLAMP_MIN);
	unsigned int max = uclamp_eff_value(donor, UCLAMP_MAX);

	p_min = clamp_t(unsigned long, p_util, p_min, p_max);
	min = clamp_t(unsigned long, donor_util, min, max);

	clamp[UCLAMP_MIN] = min > p_min ? min : 0;
	clamp[UCLAMP_MAX] = max > p_max || clamp[UCLAMP_MIN] > p_max ?
			    max : SCHED_CAPACITY_SCALE;

	trace_msched_uclamp_inheritance_result(p->pid, donor->pid,
		p_util, p_min, p_max, donor_util, min, max,
		clamp[UCLAMP_MIN], clamp[UCLAMP_MAX], type);
}

void msched_uclamp_vh_dup_task_struct(void *unused, struct task_struct *task, struct task_struct *orig) {
	enum uclamp_id clamp_id;

	if (is_enabled(UX_ENABLE_MDPF)) {
		int ux_type = task_get_ux_type(orig);
		if(ux_type & UX_TYPE_MDPF) {
			/* reset uclamp min value of new forked task */
			cond_trace_printk(unlikely(is_debuggable(DEBUG_MDPF)),
				"reset uclamp min[%d,%d] of task(tgid-%d pid-%d util-%lu) which is forked from MDPF task(tgid-%d pid-%d util-%lu)!\n",
				task->uclamp[UCLAMP_MIN].value, task->uclamp_req[UCLAMP_MIN].value,
				task->tgid, task->pid, moto_task_util(task),
				orig->tgid, orig->pid, moto_task_util(orig));

			for_each_clamp_id(clamp_id) {
				uclamp_se_set(&task->uclamp_req[clamp_id],
						uclamp_none(clamp_id), false);
			}
		}
	}
}

struct moto_binder_inherit {
	struct hlist_node node;
	struct moto_binder_inherit *next;
	struct binder_transaction *txn;
	struct task_struct *task;
	u16 clamp[UCLAMP_CNT];
	bool ux;
	bool mdpf;
};

static DEFINE_HASHTABLE(moto_binder_transactions, 8);
static DEFINE_RAW_SPINLOCK(moto_binder_lock);

static void msched_binder_put(struct moto_binder_inherit *entries)
{
	while (entries) {
		struct moto_binder_inherit *entry = entries;

		entries = entry->next;
		put_task_struct(entry->task);
		kfree(entry);
	}
}

/* The task's rq lock protects its inheritance list and clamp state. */
static void msched_binder_update(struct rq *rq, struct task_struct *task)
{
	struct moto_task_struct *mts = get_moto_task_struct(task);
	struct moto_binder_inherit *it, *clamp = NULL;
	bool ux = false;

	for (it = mts->binder_inherit; it; it = it->next) {
		ux |= it->ux;
		if (it->mdpf)
			clamp = it;
	}
	if (ux)
		task_add_ux_type(task, UX_TYPE_INHERIT_BINDER);
	else
		task_clr_ux_type(task, UX_TYPE_INHERIT_BINDER);
	WRITE_ONCE(mts->binder_clamp, clamp ? msched_pack_clamp(clamp->clamp) : 0);
	sched_moto_reinc_uclamp(rq, task);
}

void msched_binder_set(struct binder_transaction *t, struct task_struct *task, bool ux)
{
	struct moto_task_struct *mts = get_moto_task_struct(task);
	struct moto_binder_inherit *entry;
	struct rq_flags rf;
	struct rq *rq;
	bool mdpf = is_enabled(UX_ENABLE_MDPF);

	if (!ux && !mdpf)
		return;
	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return;

	get_task_struct(task);
	entry->task = task;
	entry->txn = t;
	entry->ux = ux;
	entry->mdpf = mdpf;

	rq = task_rq_lock(task, &rf);
	if (mdpf)
		msched_inherit_clamp(task, current, entry->clamp,
				    VENDOR_INHERITANCE_BINDER);

	entry->next = mts->binder_inherit;
	mts->binder_inherit = entry;
	raw_spin_lock(&moto_binder_lock);
	hash_add(moto_binder_transactions, &entry->node, (unsigned long)t);
	raw_spin_unlock(&moto_binder_lock);
	msched_binder_update(rq, task);
	task_rq_unlock(rq, task, &rf);
}

/* Caller holds the rq lock. A concurrent free may already own an entry. */
static struct moto_binder_inherit *msched_binder_prune(struct task_struct *task,
						      int dropped)
{
	struct moto_binder_inherit **link = &get_moto_task_struct(task)->binder_inherit;
	struct moto_binder_inherit *entry, *removed = NULL;

	raw_spin_lock(&moto_binder_lock);
	while ((entry = *link)) {
		if (dropped & UX_ENABLE_BINDER)
			entry->ux = false;
		if (dropped & UX_ENABLE_MDPF)
			entry->mdpf = false;
		if (entry->ux || entry->mdpf) {
			link = &entry->next;
			continue;
		}
		*link = entry->next;
		if (!hlist_unhashed(&entry->node)) {
			hash_del(&entry->node);
			entry->next = removed;
			removed = entry;
		}
	}
	raw_spin_unlock(&moto_binder_lock);
	return removed;
}

static void msched_binder_free(struct binder_transaction *t)
{
	struct moto_binder_inherit *entry, **link;
	struct task_struct *task;
	struct rq_flags rf;
	struct rq *rq;
	unsigned long flags;

	raw_spin_lock_irqsave(&moto_binder_lock, flags);
	hash_for_each_possible(moto_binder_transactions, entry, node, (unsigned long)t) {
		if (entry->txn == t) {
			hash_del(&entry->node);
			raw_spin_unlock_irqrestore(&moto_binder_lock, flags);
			goto found;
		}
	}
	raw_spin_unlock_irqrestore(&moto_binder_lock, flags);
	return;

found:
	task = entry->task;
	rq = task_rq_lock(task, &rf);
	for (link = &get_moto_task_struct(task)->binder_inherit; *link;
	     link = &(*link)->next) {
		if (*link == entry) {
			*link = entry->next;
			msched_binder_update(rq, task);
			break;
		}
	}
	task_rq_unlock(rq, task, &rf);
	put_task_struct(task);
	kfree(entry);
}

void msched_binder_restore(struct binder_transaction *t, struct task_struct *task)
{
	struct moto_binder_inherit *removed = NULL;
	struct rq_flags rf;
	struct rq *rq;

	if (t) {
		msched_binder_free(t);
		return;
	}
	if (!(moto_sched_enabled & (UX_ENABLE_BINDER | UX_ENABLE_MDPF)))
		return;
	rq = task_rq_lock(task, &rf);
	if (get_moto_task_struct(task)->binder_inherit) {
		removed = msched_binder_prune(task, UX_ENABLE_BINDER | UX_ENABLE_MDPF);
		msched_binder_update(rq, task);
	}
	task_clr_ux_type(task, UX_TYPE_LOW_LATENCY_BINDER);
	task_rq_unlock(rq, task, &rf);
	msched_binder_put(removed);
}

static void msched_binder_free_hook(void *unused, struct binder_transaction *t)
{
	msched_binder_free(t);
}

int msched_binder_register(void)
{
	return register_trace_android_vh_free_oem_binder_struct(msched_binder_free_hook, NULL);
}

void msched_binder_unregister(void)
{
	unregister_trace_android_vh_free_oem_binder_struct(msched_binder_free_hook, NULL);
	tracepoint_synchronize_unregister();
}

void msched_mask_changed(int old_mask, int new_mask)
{
	const int dropped = (old_mask & ~new_mask) & (UX_ENABLE_BINDER | UX_ENABLE_MDPF);
	struct task_struct *group, *task;

	if (!dropped)
		return;

	rcu_read_lock();
	for_each_process_thread(group, task) {
		struct moto_task_struct *mts = get_moto_task_struct(task);
		struct moto_binder_inherit *removed;
		struct rq_flags rf;
		struct rq *rq = task_rq_lock(task, &rf);

		if (dropped & UX_ENABLE_BINDER)
			task_clr_ux_type(task, UX_TYPE_LOW_LATENCY_BINDER);
		if (dropped & UX_ENABLE_MDPF)
			WRITE_ONCE(mts->pi_clamp, 0);
		removed = msched_binder_prune(task, dropped);
		msched_binder_update(rq, task);
		task_rq_unlock(rq, task, &rf);
		msched_binder_put(removed);
	}
	rcu_read_unlock();
}

static void rvh_rtmutex_prepare_setprio(void *data, struct task_struct *p,
				       struct task_struct *pi_task)
{
	struct moto_task_struct *mts = get_moto_task_struct(p);
	struct rq_flags rf;
	struct rq *rq;

	if (!is_enabled(UX_ENABLE_MDPF))
		return;
	lockdep_assert_held(&p->pi_lock);
	rq = __task_rq_lock(p, &rf);
	WRITE_ONCE(mts->pi_clamp, 0);
	if (pi_task) {
		u16 clamp[UCLAMP_CNT];

		msched_inherit_clamp(p, pi_task, clamp, VENDOR_INHERITANCE_RTMUTEX);
		WRITE_ONCE(mts->pi_clamp, msched_pack_clamp(clamp));
	}
	sched_moto_reinc_uclamp(rq, p);
	__task_rq_unlock(rq, &rf);
}

static void rvh_sched_fork_init(void *unused, struct task_struct *p) {
	struct moto_task_struct *mts = get_moto_task_struct(p);
	mts->binder_clamp = 0;
	mts->pi_clamp = 0;
	mts->binder_inherit = NULL;
}

static bool msched_uclamp_ready;

int msched_validate_mask(int mask)
{
	if ((mask & UX_ENABLE_MDPF) && !smp_load_acquire(&msched_uclamp_ready))
		return -ENODEV;
	return 0;
}

int msched_uclamp_register_vendor_comm_hooks(void)
{
	int ret;

	ret = register_trace_android_rvh_rtmutex_prepare_setprio(
		rvh_rtmutex_prepare_setprio, NULL);
	if (!ret)
		ret = register_trace_android_rvh_uclamp_eff_get(rvh_uclamp_eff_get, NULL);
	if (!ret)
		ret = register_trace_android_rvh_sched_fork_init(rvh_sched_fork_init, NULL);
	if (!ret)
		smp_store_release(&msched_uclamp_ready, true);

	return ret;
}
