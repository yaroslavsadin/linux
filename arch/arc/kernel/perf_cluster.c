// SPDX-License-Identifier: GPL-2.0+
//
// Linux cluster performance counters support for ARCv3.
//
// Copyright (C) 2013-2022 Synopsys, Inc. (www.synopsys.com)


#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/perf_event.h>
#include <asm/arcregs.h>
#include <asm/stacktrace.h>
#include <asm/perf_cluster_event.h>


enum arc_pmu_attr_groups {
	ARCPMU_ATTR_GR_EVENTS,
	ARCPMU_ATTR_GR_FORMATS,
	ARCPMU_NR_ATTR_GR
};

struct arc_cluster_pmu {
	struct pmu	pmu;
	unsigned int	irq;
	int		n_counters;
	int		n_events;
	u64		max_period;
	unsigned long	used_mask[BITS_TO_LONGS(ARC_CLUSTER_PERF_MAX_COUNTERS)];

	struct cpct_conditions_entry	*raw_entry;
	struct attribute		**attrs;
	struct perf_pmu_events_attr	*attr;
	const struct attribute_group	*attr_groups[ARCPMU_NR_ATTR_GR + 1];
};

static struct arc_cluster_pmu *arc_cluster_pmu;

static ssize_t arc_pmu_events_sysfs_show(struct device *dev,
					 struct device_attribute *attr,
					 char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sprintf(page, "event=0x%04llx\n", pmu_attr->id);
}

static void arc_cluster_pmu_add_raw_event_attr(int j)
{
	arc_cluster_pmu->attr[j].attr.attr.name = arc_cluster_pmu->raw_entry[j].name.cc;
	arc_cluster_pmu->attr[j].attr.attr.mode = VERIFY_OCTAL_PERMISSIONS(0444);
	arc_cluster_pmu->attr[j].attr.show = arc_pmu_events_sysfs_show;
	arc_cluster_pmu->attr[j].id = j;
	arc_cluster_pmu->attrs[j] = &(arc_cluster_pmu->attr[j].attr.attr);
}

static void arc_cluster_pmu_read_reg(unsigned int reg, void *data)
{
    unsigned int val;
    WRITE_AUX(CLNR_ADDR, reg);
    READ_BCR(CLNR_DATA, val);
    *(unsigned int *)data = val;
}

static void arc_cluster_pmu_write_reg(unsigned int reg, void *data)
{
    unsigned int val = *(unsigned int *)data;
    WRITE_AUX(CLNR_ADDR, reg);
    WRITE_AUX(CLNR_DATA, val);
}

/* read counter #idx; note that counter# != event# on ARC! */
static u64 arc_cluster_pmu_read_counter(int idx)
{
    struct cpct_snap snapL, snapH;
	struct cpct_n_config cfg;
	u64 result;
	int number;

	/*
	 * ARC cluster supports making 'snapshots' of the counters, so we don't
	 * need to care about counters wrapping to 0 underneath our feet
	 */
    number = 8 * idx; // select counter, 0..31

	arc_cluster_pmu_read_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);
    cfg.lsn = 1; // take snapshot
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);

	arc_cluster_pmu_read_reg(SCM_AUX_CPCT_N_SNAPH + number, &snapH);
    arc_cluster_pmu_read_reg(SCM_AUX_CPCT_N_SNAPL + number, &snapL);
    result = ((u64)snapH.snap << 32ULL) | (u64)snapL.snap;
	return result;
}

static void arc_cluster_perf_event_update(struct perf_event *event,
				  struct hw_perf_event *hwc, int idx)
{
	u64 prev_raw_count = local64_read(&hwc->prev_count);
	u64 new_raw_count = arc_cluster_pmu_read_counter(idx);
	s64 delta = new_raw_count - prev_raw_count;

	/*
	 * We aren't afraid of hwc->prev_count changing beneath our feet
	 * because there's no way for us to re-enter this function anytime.
	 */
	local64_set(&hwc->prev_count, new_raw_count);
	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);
}

static void arc_cluster_pmu_read(struct perf_event *event)
{
	arc_cluster_perf_event_update(event, &event->hw, event->hw.idx);
}

/* initializes hw_perf_event structure if event is supported */
static int arc_cluster_pmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cpct_n_config cfg;

	if (!is_sampling_event(event)) {
		hwc->sample_period = arc_cluster_pmu->max_period;
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	switch (event->attr.type) {
	case PERF_TYPE_RAW:
	case PERF_TYPE_MAX:
		if (event->attr.config >= arc_cluster_pmu->n_events) {
			return -ENOENT;
		}
		cfg.val = 0;
		cfg.cc_num = arc_cluster_pmu->raw_entry[event->attr.config].cc_number;
		cfg.lce = 1;
		hwc->config = cfg.val;
		return 0;
	default:
		return -ENOENT;
	}
}

/* starts all counters */
static void arc_cluster_pmu_enable(struct pmu *pmu)
{
	struct cpct_n_config cfg;
	int ii;
	int number;
	int N = arc_cluster_pmu->n_counters;

	for (ii = 0; ii < N; ii++) {
		number = ii * 8;
		arc_cluster_pmu_read_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);
		if (cfg.cc_num != 0xFFFF && cfg.lce == 1) {
			cfg.len = 1;
			arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);
		}
	}
}

/* stops all counters */
static void arc_cluster_pmu_disable(struct pmu *pmu)
{
	struct cpct_n_config cfg;
	int ii;
	int number;
	int N = arc_cluster_pmu->n_counters;

	for (ii = 0; ii < N; ii++) {
		number = ii * 8;
		arc_cluster_pmu_read_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);
		if (cfg.cc_num != 0xFFFF && cfg.lce == 1) {
			cfg.len = 0;
			arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &cfg);
		}
	}
}

static int arc_pmu_event_set_period(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int idx = hwc->idx;
	int overflow = 0;
	u64 value;
	u32 vv;
	int number;

	if (unlikely(left <= -period)) {
		/* left underflowed by more than period. */
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		overflow = 1;
	} else if (unlikely(left <= 0)) {
		/* left underflowed by less than period. */
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		overflow = 1;
	}

	if (left > arc_cluster_pmu->max_period)
		left = arc_cluster_pmu->max_period;

	value = arc_cluster_pmu->max_period - left;
	local64_set(&hwc->prev_count, value);

	/* Select counter */
	number = 8 * idx; // select counter, 0..31

	/* Write value */
	vv = (u32)value;
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_COUNTL + number, &vv);
	vv = (u32)(value >> 32ULL);
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_COUNTH + number, &vv);

	perf_event_update_userpage(event);

	return overflow;
}

/*
 * Assigns hardware counter to hardware condition.
 * Note that there is no separate start/stop mechanism;
 * stopping is achieved by assigning the 'never' condition
 */
static void arc_cluster_pmu_start(struct perf_event *event, int flags)
{
	
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	int number = 8 * idx;
	u32 vv;

	if (WARN_ON_ONCE(idx == -1))
		return;

	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;

	arc_pmu_event_set_period(event);

	/* Enable interrupt for this counter */
	if (is_sampling_event(event)) {
		arc_cluster_pmu_read_reg(SCM_AUX_CPCT_INT_CTRL, &vv);
		vv |= BIT(idx);
		arc_cluster_pmu_write_reg(SCM_AUX_CPCT_INT_CTRL, &vv);
	}

	/* enable ARC pmu here */
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &hwc->config);
}

static void arc_cluster_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	u32 vv;

	/* Disable interrupt for this counter */
	if (is_sampling_event(event)) {
		/*
		 * Reset interrupt flag by writing of 1. This is required
		 * to make sure pending interrupt was not left.
		 * This also clears enable bit for the SCM_AUX_CPCT_INT_CTRL.
		 */
		vv = BIT(idx);
		arc_cluster_pmu_write_reg(SCM_AUX_CPCT_INT_ACT, &vv);
	}

	if (!(event->hw.state & PERF_HES_STOPPED)) {
		event->hw.state |= PERF_HES_STOPPED;
	}

	if ((flags & PERF_EF_UPDATE) &&
	    !(event->hw.state & PERF_HES_UPTODATE)) {
		arc_cluster_perf_event_update(event, &event->hw, idx);
		event->hw.state |= PERF_HES_UPTODATE;
	}
}

/* allocate hardware counter and optionally start counting */
static int arc_cluster_pmu_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	int number;
	u32 vv;

	idx = ffz(arc_cluster_pmu->used_mask[0]);
	if (idx == arc_cluster_pmu->n_counters) {
		return -EAGAIN;
	}

	__set_bit(idx, arc_cluster_pmu->used_mask);
	hwc->idx = idx;

	number = idx * 8;

	if (is_sampling_event(event)) {
		/* Mimic full counter overflow as other arches do */
		vv = (u32)arc_cluster_pmu->max_period;
		arc_cluster_pmu_write_reg(SCM_AUX_CPCT_INT_CNTL + number, &vv);
		vv = (u32)(arc_cluster_pmu->max_period >> 32ULL);
		arc_cluster_pmu_write_reg(SCM_AUX_CPCT_INT_CNTH + number, &vv);
	}

	vv = 0;
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &vv);
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_COUNTL + number, &vv);
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_COUNTH + number, &vv);
	local64_set(&hwc->prev_count, 0);

	hwc->state = PERF_HES_UPTODATE | PERF_HES_STOPPED;
	if (flags & PERF_EF_START)
		arc_cluster_pmu_start(event, PERF_EF_RELOAD);

	perf_event_update_userpage(event);

	return 0;
}

static void arc_cluster_pmu_del(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	int number = 8 * idx;
	unsigned int vv;

	arc_cluster_pmu_stop(event, PERF_EF_UPDATE);
	__clear_bit(event->hw.idx, arc_cluster_pmu->used_mask);

    vv = 0;
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_N_CONFIG + number, &vv);

	perf_event_update_userpage(event);
}

/* Event field occupies the bottom 16 bits of our config field */
PMU_FORMAT_ATTR(event, "config:0-15");
static struct attribute *arc_cluster_pmu_format_attrs[] = {
	&format_attr_event.attr,
	NULL,
};

static struct attribute_group arc_cluster_pmu_format_attr_gr = {
	.name = "format",
	.attrs = arc_cluster_pmu_format_attrs,
};

/*
 * We don't add attrs here as we don't have pre-defined list of perf events.
 * We will generate and add attrs dynamically in probe() after we read HW
 * configuration.
 */
static struct attribute_group arc_cluster_pmu_events_attr_gr = {
	.name = "events",
};

static int arc_cluster_pmu_raw_alloc(struct device *dev)
{
	arc_cluster_pmu->attr = devm_kmalloc_array(dev, arc_cluster_pmu->n_events + 1,
		sizeof(*arc_cluster_pmu->attr), GFP_KERNEL | __GFP_ZERO);
	if (!arc_cluster_pmu->attr)
		return -ENOMEM;

	arc_cluster_pmu->attrs = devm_kmalloc_array(dev, arc_cluster_pmu->n_events + 1,
		sizeof(*arc_cluster_pmu->attrs), GFP_KERNEL | __GFP_ZERO);
	if (!arc_cluster_pmu->attrs) {
		devm_kfree(dev, arc_cluster_pmu->attr);
		return -ENOMEM;
	}

	arc_cluster_pmu->raw_entry = devm_kmalloc_array(dev, arc_cluster_pmu->n_events,
		sizeof(*arc_cluster_pmu->raw_entry), GFP_KERNEL | __GFP_ZERO);
	if (!arc_cluster_pmu->raw_entry) {
		devm_kfree(dev, arc_cluster_pmu->attr);
		devm_kfree(dev, arc_cluster_pmu->attrs);
		return -ENOMEM;
	}

	return 0;
}

static void arc_cluster_pmu_raw_free(struct device *dev)
{
	devm_kfree(dev, arc_cluster_pmu->attr);
	devm_kfree(dev, arc_cluster_pmu->attrs);
	devm_kfree(dev, arc_cluster_pmu->raw_entry);
}

static int arc_cluster_pmu_is_present(void)
{
    struct cpct_build bld;
    arc_cluster_pmu_read_reg(SCM_AUX_CPCT_BUILD, &bld);
    return bld.ver;
}

static int arc_cluster_pmu_get_n_counters(void)
{
    struct cpct_build bld;
    arc_cluster_pmu_read_reg(SCM_AUX_CPCT_BUILD, &bld);
    return bld.num_ctrs;
}

static int arc_cluster_pmu_has_interrupt(void)
{
    struct cpct_build bld;
    arc_cluster_pmu_read_reg(SCM_AUX_CPCT_BUILD, &bld);
    return bld.i;
}

static int arc_cluster_pmu_get_cnt_bits(void)
{
    struct cpct_build bld;
    arc_cluster_pmu_read_reg(SCM_AUX_CPCT_BUILD, &bld);
    return 32 + 16 * bld.cs;
}

static int arc_cluster_pmu_get_events_number(void)
{
    int ii;
    struct cpct_cc_num cc_num;
    union cpct_cc_name name;
    int n_actual_conditions = 0;

    name.cc[CPCT_NAME_SZ-1] = 0;
    for (ii = 0; ii < MAX_CONDITIONS_NUMBER; ii++ ) {
        cc_num.cc_num = ii;
        cc_num.res = 0; // recomended to write with 0
        arc_cluster_pmu_write_reg(SCM_AUX_CPCT_CC_NUM, &cc_num);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME0, &name.uu[0]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME1, &name.uu[1]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME2, &name.uu[2]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME3, &name.uu[3]);
        if (strlen(name.cc) == 0) {
            continue;
        }
        n_actual_conditions++;
    }
    return n_actual_conditions;
}

static int arc_cluster_pmu_fill_events(void)
{
    int ii;
    struct cpct_cc_num cc_num;
    union cpct_cc_name name;
    int n_actual_conditions = 0;

    name.cc[CPCT_NAME_SZ-1] = 0;
    for (ii = 0; ii < MAX_CONDITIONS_NUMBER; ii++ ) {
        cc_num.cc_num = ii;
        cc_num.res = 0; // recomended to write with 0
        arc_cluster_pmu_write_reg(SCM_AUX_CPCT_CC_NUM, &cc_num);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME0, &name.uu[0]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME1, &name.uu[1]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME2, &name.uu[2]);
        arc_cluster_pmu_read_reg(SCM_AUX_CPCT_CC_NAME3, &name.uu[3]);
        if (strlen(name.cc) == 0) {
            continue;
        }

		arc_cluster_pmu->raw_entry[n_actual_conditions].cc_number = ii;
        strcpy(arc_cluster_pmu->raw_entry[n_actual_conditions].name.cc, name.cc);

        n_actual_conditions++;
		if (n_actual_conditions >= arc_cluster_pmu->n_events) {
			break;
		}
    }

    return n_actual_conditions;
}

static int arc_cluster_pmu_device_probe(struct platform_device *pdev)
{
	int i;
	int has_interrupts;
	int counter_size;	/* in bits */
    int ncounters;
    int nevents;
	unsigned int int_reg;

#ifndef CONFIG_ISA_ARCV3
	pr_err("Cluster PMU driver must be used only on ARCv3 platform!\n");
	return -ENODEV;
#endif

    if (!arc_cluster_pmu_is_present()){
        pr_err("Error! Cluster PCT module is not presented in the system.\n");
        return -ENODEV;
    }

    ncounters = arc_cluster_pmu_get_n_counters();
    if (!ncounters) {
        pr_err("Error! Cluster PCT module doesn't have counters.\n");
        return -EINVAL;
    }

	nevents = arc_cluster_pmu_get_events_number();
    if (!nevents) {
        pr_err("Error! Cluster PCT module doesn't have countable conditions.\n");
        return -EINVAL;
    }

	arc_cluster_pmu = devm_kzalloc(&pdev->dev, sizeof(struct arc_cluster_pmu), GFP_KERNEL);
	if (!arc_cluster_pmu)
		return -ENOMEM;

	arc_cluster_pmu->n_counters = ncounters;
	arc_cluster_pmu->n_events = nevents;
	counter_size = arc_cluster_pmu_get_cnt_bits();
	arc_cluster_pmu->max_period = (1ULL << counter_size) / 2 - 1ULL;
	has_interrupts = arc_cluster_pmu_has_interrupt();

	pr_info("ARCv3 cluster perf\t: %d counters (%d bits), %d conditions%s\n",
		arc_cluster_pmu->n_counters, counter_size, arc_cluster_pmu->n_events,
		has_interrupts ? ", [overflow IRQ support]" : "");

	if (arc_cluster_pmu_raw_alloc(&pdev->dev)) {
		devm_kfree(&pdev->dev, arc_cluster_pmu);
		return -ENOMEM;
	}

	if (arc_cluster_pmu_fill_events() == 0 ) {
		pr_err("Error! While preparing events structure.\n");
		arc_cluster_pmu_raw_free(&pdev->dev);
		return -EINVAL;
	}

	/* loop thru all available h/w condition indexes */
	for (i = 0; i < nevents; i++) {
		arc_cluster_pmu_add_raw_event_attr(i);
	}

	arc_cluster_pmu_events_attr_gr.attrs = arc_cluster_pmu->attrs;
	arc_cluster_pmu->attr_groups[ARCPMU_ATTR_GR_EVENTS] = &arc_cluster_pmu_events_attr_gr;
	arc_cluster_pmu->attr_groups[ARCPMU_ATTR_GR_FORMATS] = &arc_cluster_pmu_format_attr_gr;

	arc_cluster_pmu->pmu = (struct pmu) {
		.pmu_enable	= arc_cluster_pmu_enable,
		.pmu_disable	= arc_cluster_pmu_disable,
		.event_init	= arc_cluster_pmu_event_init,
		.add		= arc_cluster_pmu_add,
		.del		= arc_cluster_pmu_del,
		.start		= arc_cluster_pmu_start,
		.stop		= arc_cluster_pmu_stop,
		.read		= arc_cluster_pmu_read,
		.attr_groups	= arc_cluster_pmu->attr_groups,
	};

	arc_cluster_pmu->pmu.capabilities |= PERF_PMU_CAP_NO_INTERRUPT | PERF_PMU_CAP_HETEROGENEOUS_CPUS;
	int_reg = 0;
	arc_cluster_pmu_write_reg(SCM_AUX_CPCT_INT_CTRL, &int_reg);

	/*
	 * perf parser doesn't really like '-' symbol in events name, so let's
	 * use '_' in arc pct name as it goes to kernel PMU event prefix.
	 */
	return perf_pmu_register(&arc_cluster_pmu->pmu, "arc_cluster_pct", PERF_TYPE_MAX);
}

static const struct of_device_id arc_cluster_pmu_match[] = {
	{ .compatible = "snps,arcv3-cluster-pmu" },
	{},
};
MODULE_DEVICE_TABLE(of, arc_cluster_pmu_match);

static struct platform_driver arc_cluster_pmu_driver = {
	.driver	= {
		.name		= "ARCv3-cluster-PMU",
		.of_match_table = of_match_ptr(arc_cluster_pmu_match),
	},
	.probe		= arc_cluster_pmu_device_probe,
};

module_platform_driver(arc_cluster_pmu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Synopsys");
MODULE_DESCRIPTION("ARCv3 cluster PMU driver");

