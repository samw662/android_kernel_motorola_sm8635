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

#ifndef _MOTO_SCHED_SYSFS_H_
#define _MOTO_SCHED_SYSFS_H_

int moto_sched_proc_init(void);
void moto_sched_proc_deinit(void);
int moto_sched_register_ops(void);
void moto_sched_enable_writes(void);
void msched_mask_changed(int old_mask, int new_mask);
int msched_validate_mask(int mask);
int msched_binder_register(void);
void msched_binder_unregister(void);


extern struct task_struct *find_task_by_vpid(pid_t vnr);

#endif /* _MOTO_SCHED_SYSFS_H_ */
