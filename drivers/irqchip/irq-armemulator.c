/*
 * ARM emulator IRQ chip driver.
 *
 * Copyright (C) 2019 hxdyxd
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/exception.h>

//Determine which interrupt source is masked.
#define INTMSK_OFFSET    0x00
//Indicate the interrupt request status
#define INTPND_OFFSET    0x04

struct armemulator_irq_chip_data {
	void __iomem *irq_base;
	struct irq_domain *irq_domain;
};


static struct armemulator_irq_chip_data *irq_ic_data;

static void __exception_irq_entry armemulator_handle_irq(struct pt_regs *regs);



static void armemulator_irq_ack(struct irq_data *irqd)
{
	unsigned int irq = irqd_to_hwirq(irqd);
	uint32_t val;

	val = readl(irq_ic_data->irq_base + INTPND_OFFSET);
	writel(val & ~(1 << irq), irq_ic_data->irq_base + INTPND_OFFSET);
}

static void armemulator_irq_mask(struct irq_data *irqd)
{
	unsigned int irq = irqd_to_hwirq(irqd);
	uint32_t val;

	val = readl(irq_ic_data->irq_base + INTMSK_OFFSET);
	writel(val | (1 << irq), irq_ic_data->irq_base + INTMSK_OFFSET);
}

static void armemulator_irq_unmask(struct irq_data *irqd)
{
	unsigned int irq = irqd_to_hwirq(irqd);
	uint32_t val;

	val = readl(irq_ic_data->irq_base + INTMSK_OFFSET);
	writel(val & ~(1 << irq), irq_ic_data->irq_base + INTMSK_OFFSET);
}

static struct irq_chip armemulator_irq_chip = {
	.name		= "armemulator_irq",
	.irq_eoi	= armemulator_irq_ack,
	.irq_mask	= armemulator_irq_mask,
	.irq_unmask	= armemulator_irq_unmask,
	.flags		= IRQCHIP_EOI_THREADED | IRQCHIP_EOI_IF_HANDLED,
};





static int armemulator_irq_map(struct irq_domain *d, unsigned int virq, irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &armemulator_irq_chip, handle_fasteoi_irq);
	irq_set_probe(virq);

	return 0;
}

static const struct irq_domain_ops armemulator_irq_ops = {
	.map = armemulator_irq_map,
	.xlate = irq_domain_xlate_onecell,
};



static int __init armemulator_of_init(struct device_node *node,
				struct device_node *parent)
{
	irq_ic_data->irq_base = of_iomap(node, 0);
	if (!irq_ic_data->irq_base)
		panic("%pOF: unable to map IC registers\n",
			node);

	printk("armemulator-ic base address = 0x%x\n", (uint32_t)irq_ic_data->irq_base);

	/* mask all the interrupts */
	writel(0xffffffff, irq_ic_data->irq_base + INTMSK_OFFSET);

	/* Clear all the pending interrupts */
	writel(0, irq_ic_data->irq_base + INTPND_OFFSET);

	irq_ic_data->irq_domain = irq_domain_add_linear(node, 3 * 32,
						 &armemulator_irq_ops, NULL);
	if (!irq_ic_data->irq_domain)
		panic("%pOF: unable to create IRQ domain\n", node);

	set_handle_irq(armemulator_handle_irq);

	return 0;
}



static int __init armemulator_ic_of_init(struct device_node *node, struct device_node *parent)
{
	irq_ic_data = kzalloc(sizeof(struct armemulator_irq_chip_data), GFP_KERNEL);
	if (!irq_ic_data) {
		pr_err("kzalloc failed!\n");
		return -ENOMEM;
	}

	//irq_ic_data->enable_reg_offset = SUNIV_IRQ_ENABLE_REG_OFFSET;
	//irq_ic_data->mask_reg_offset = SUNIV_IRQ_MASK_REG_OFFSET;

	return armemulator_of_init(node, parent);
}

IRQCHIP_DECLARE(armemulator_ic, "hxdyxd,armemulator-ic", armemulator_ic_of_init);


static void __exception_irq_entry armemulator_handle_irq(struct pt_regs *regs)
{
	int i;
	u32 hwirq;

	hwirq = readl(irq_ic_data->irq_base + INTPND_OFFSET);
	for(i=0; i<32; i++) {
		if( (1<<i)&hwirq ) {
			handle_domain_irq(irq_ic_data->irq_domain, i, regs);
			return;
		}
	}
}

