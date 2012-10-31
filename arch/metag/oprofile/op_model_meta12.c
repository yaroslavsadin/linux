/*
 *  Meta version derived from arch/sh/oprofile/op_model_sh7750.c
 *    Copyright (C) 2008 Imagination Technologies Ltd.
 *
 * arch/sh/oprofile/op_model_sh7750.c
 *
 * OProfile support for SH7750/SH7750S Performance Counters
 *
 * Copyright (C) 2003, 2004  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/oprofile.h>
#include <linux/profile.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define METAC_PERF_VALUES
#include <asm/tbx/machine.inc>

#include "backtrace.h"

/*
 * Meta has 2 perf counters
 */
#define NR_CNTRS	2

struct op_counter_config {
	unsigned long enabled;
	unsigned long event;
	unsigned long count;
	unsigned long unit_mask;

	/* Dummy values for userspace tool compliance */
	unsigned long kernel;
	unsigned long user;
};

static struct op_counter_config ctr[NR_CNTRS];

static u32 meta_read_counter(int counter)
{
	u32 val = readl(counter ? PERF_COUNT0 : PERF_COUNT1);
	return val;
}

static void meta_write_counter(int counter, u32 val)
{
	writel(val, counter ? PERF_COUNT0 : PERF_COUNT1);
}

/*
 * Unfortunately we don't have a native exception or interrupt for counter
 * overflow.
 *
 * OProfile on the other hand likes to have samples taken periodically, so
 * for now we just piggyback the timer interrupt to get the expected
 * behavior.
 */

static int meta_timer_notify(struct pt_regs *regs)
{
	int i;
	u32 val, total_val, sub_val;
	u32 enabled_threads;

	for (i = 0; i < NR_CNTRS; i++) {
		if (!ctr[i].enabled)
			continue;

		/* Disable performance monitoring. */
		enabled_threads = meta_read_counter(i);
		meta_write_counter(i, 0);

		sub_val = total_val = val = enabled_threads & PERF_COUNT_BITS;

		if (val >= ctr[i].count) {
			while (val > ctr[i].count) {
				oprofile_add_sample(regs, i);
				val -= ctr[i].count;
			}
			/* val may be < ctr[i].count but > 0 */
			sub_val -= val;
			total_val -= sub_val;
		}

		/* Enable performance monitoring. */
		enabled_threads &= (PERF_CTRL_BITS | PERF_THREAD_BITS);
		enabled_threads = enabled_threads | total_val;
		meta_write_counter(i, enabled_threads);
	}

	return 0;
}

/*
 * Files will be in a path like:
 *
 *  /<oprofilefs mount point>/<counter number>/<file>
 *
 * So when dealing with <file>, we look to the parent dentry for the counter
 * number.
 */
static inline int to_counter(struct file *file)
{
	long val;
	const unsigned char *name = file->f_path.dentry->d_parent->d_name.name;

	if (kstrtol(name, 10, &val))
		return 0;

	return val;
}

static ssize_t meta_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos, int mask, int shift)
{
	int counter = to_counter(file);
	unsigned long write_val;
	u32 read_val;

	if (oprofilefs_ulong_from_user(&write_val, buf, count))
		return -EFAULT;

	read_val = meta_read_counter(counter) & ~mask;
	write_val <<= shift;

	write_val = read_val | (write_val & mask);
	meta_write_counter(counter, write_val);

	return count;
}

/*
 * These functions handle turning perfomance counters on for particular
 * threads by writing to files.
 */
static ssize_t meta_read_thread(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	int counter = to_counter(file);
	u32 val = meta_read_counter(counter);

	val &= PERF_THREAD_BITS;
	val >>= PERF_THREAD_S;

	return oprofilefs_ulong_to_user((unsigned long)val, buf, count, ppos);
}

static ssize_t meta_write_thread(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	meta_write(file, buf, count, ppos, PERF_THREAD_BITS, PERF_THREAD_S);
	return count;
}

static const struct file_operations thread_fops = {
	.read		= meta_read_thread,
	.write		= meta_write_thread,
};

static int meta_perf_counter_create_files(struct super_block *sb,
					  struct dentry *root)
{
	int i;

	for (i = 0; i < NR_CNTRS; i++) {
		struct dentry *dir;
		char buf[4];

		snprintf(buf, sizeof(buf), "%d", i);
		dir = oprofilefs_mkdir(sb, root, buf);

		oprofilefs_create_ulong(sb, dir, "enabled", &ctr[i].enabled);
		oprofilefs_create_ulong(sb, dir, "event", &ctr[i].event);
		oprofilefs_create_ulong(sb, dir, "count", &ctr[i].count);
		oprofilefs_create_file(sb, dir, "unit_mask", &thread_fops);

		/* Dummy entries */
		oprofilefs_create_ulong(sb, dir, "kernel", &ctr[i].kernel);
		oprofilefs_create_ulong(sb, dir, "user", &ctr[i].user);
	}

	return 0;
}

static int meta_perf_counter_start(void)
{
	int i;
	u32 event, read_val;

	for (i = 0; i < NR_CNTRS; i++) {
		if (!ctr[i].enabled)
			continue;

		event = ctr[i].event << PERF_CTRL_S;
		read_val = meta_read_counter(i) & ~PERF_CTRL_BITS;
		meta_write_counter(i, read_val | event);
	}

	return register_timer_hook(meta_timer_notify);
}

static void meta_perf_counter_stop(void)
{
	u32 val;

	val = meta_read_counter(0) & ~PERF_THREAD_BITS;
	meta_write_counter(0, val);

	val = meta_read_counter(1) & ~PERF_THREAD_BITS;
	meta_write_counter(1, val);

	unregister_timer_hook(meta_timer_notify);
}

int __init oprofile_arch_init(struct oprofile_operations *ops)
{
	ops->cpu_type = "metag";
	ops->create_files = meta_perf_counter_create_files;
	ops->start = meta_perf_counter_start;
	ops->stop = meta_perf_counter_stop;
	ops->backtrace = metag_backtrace;

	pr_info("oprofile: using %s performance monitoring.\n", ops->cpu_type);

	/* Clear the counters. */
	meta_write_counter(0, 0);
	meta_write_counter(1, 0);

	return 0;
}

void oprofile_arch_exit(void)
{
}

