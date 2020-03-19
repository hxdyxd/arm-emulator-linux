/*
 * ARM emulator timer handling.
 *
 * Copyright (C) 2019 hxdyxd
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/sched_clock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "timer-of.h"

#define  TIMER_CNTVAL_OFFSET   0x00
#define  TIMER_ENABLE_OFFSET   0x04

#define TIMER_SYNC_TICKS	3



static void armemulator_clkevt_time_stop(void __iomem *base, u8 timer)
{
	writel(0, base  + TIMER_ENABLE_OFFSET);
}


static void armemulator_clkevt_time_start(void __iomem *base, u8 timer,
				    bool periodic)
{
	writel(1, base  + TIMER_ENABLE_OFFSET);
}

static int armemulator_clkevt_shutdown(struct clock_event_device *evt)
{
	struct timer_of *to = to_timer_of(evt);

	armemulator_clkevt_time_stop(timer_of_base(to), 0);
	return 0;
}

/*
static int armemulator_clkevt_set_oneshot(struct clock_event_device *evt)
{

	return 0;
}
*/

static int armemulator_clkevt_set_periodic(struct clock_event_device *evt)
{
	struct timer_of *to = to_timer_of(evt);


	printk("set periodic to %d\n",  (uint32_t)timer_of_period(to));
	armemulator_clkevt_time_start(timer_of_base(to), 0, true);
	return 0;
}

static int armemulator_clkevt_next_event(unsigned long evt,
				   struct clock_event_device *clkevt)
{
	printk("next event %d\n", evt);
	return 0;
}



static irqreturn_t armemulator_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = (struct clock_event_device *)dev_id;
	struct timer_of *to = to_timer_of(evt);

	//printk("timer irq %d happen\n", irq);
	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct timer_of to = {
	.flags = TIMER_OF_IRQ | TIMER_OF_CLOCK | TIMER_OF_BASE,

	.clkevt = {
		.name = "armemulator_tick",
		.rating = 350,
		.features = CLOCK_EVT_FEAT_PERIODIC,
		.set_state_shutdown = armemulator_clkevt_shutdown,
		.set_state_periodic = armemulator_clkevt_set_periodic,
		//.set_state_oneshot = armemulator_clkevt_set_oneshot,
		.tick_resume = armemulator_clkevt_shutdown,
		.set_next_event = armemulator_clkevt_next_event,
		.cpumask = cpu_possible_mask,
	},

	.of_irq = {
		.handler = armemulator_timer_interrupt,
		.flags = IRQF_TIMER | IRQF_IRQPOLL,
	},
};

static u64 notrace armemulator_timer_sched_read(void)
{
	return readl(timer_of_base(&to) + TIMER_CNTVAL_OFFSET);
}

static int __init armemulator_timer_init(struct device_node *node)
{
	int ret;
	u32 val;

	ret = timer_of_init(node, &to);
	if (ret) {
		printk("armemulator-timer error\n");
		return ret;
	}
	printk("armemulator-timer base address = 0x%x\n", (uint32_t)timer_of_base(&to));


	sched_clock_register(armemulator_timer_sched_read, 32, 1000);

	ret = clocksource_mmio_init(timer_of_base(&to) + TIMER_CNTVAL_OFFSET,
				    node->name, 1000, 350, 32,
				    clocksource_mmio_readl_up);
	if (ret) {
		pr_err("Failed to register clocksource\n");
		return ret;
	}

	/* Make sure timer is stopped before playing with interrupts */
	armemulator_clkevt_time_stop(timer_of_base(&to), 0);


	clockevents_config_and_register(&to.clkevt, timer_of_rate(&to),
					TIMER_SYNC_TICKS, 0xffffffff);

	return ret;
}

TIMER_OF_DECLARE(armemulator, "hxdyxd,armemulator-timer",
		       armemulator_timer_init);

