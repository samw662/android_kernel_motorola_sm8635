/*
 * Copyright (c) 2025 Motorola Inc.
 */

#define get_bucket_id(__val)								      \
		min_t(unsigned int,							      \
		      __val / DIV_ROUND_CLOSEST(SCHED_CAPACITY_SCALE, UCLAMP_BUCKETS),	      \
		      UCLAMP_BUCKETS - 1)

#define for_each_clamp_id(clamp_id) \
	for ((clamp_id) = 0; (clamp_id) < UCLAMP_CNT; (clamp_id)++)

#define VENDOR_INHERITANCE_BINDER 0
#define VENDOR_INHERITANCE_RTMUTEX 1

static inline unsigned int uclamp_none(enum uclamp_id clamp_id)
{
	if (clamp_id == UCLAMP_MIN)
		return 0;
	return SCHED_CAPACITY_SCALE;
}

static inline void uclamp_se_set(struct uclamp_se *uc_se,
				 unsigned int value, bool user_defined)
{
	uc_se->value = value;
	uc_se->bucket_id = get_bucket_id(value);
	uc_se->user_defined = user_defined;
}

struct binder_transaction;
void msched_binder_set(struct binder_transaction *t, struct task_struct *task, bool ux);
void msched_binder_restore(struct binder_transaction *t, struct task_struct *task);
extern void msched_uclamp_vh_dup_task_struct(void *unused, struct task_struct *task, struct task_struct *orig);
