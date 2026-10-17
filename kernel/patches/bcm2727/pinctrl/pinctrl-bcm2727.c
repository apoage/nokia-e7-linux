// SPDX-License-Identifier: GPL-2.0+
/*
 * Pin controller / GPIO driver for the Broadcom BCM2727B1 (RAPU) GENIO block.
 *
 * Nokia E7-00 (RM-626), Symbian^3 phone.  The register map below is decoded
 * from static analysis of the Symbian ROM kernel GPIO driver
 * (docs/gpio-static-pinmux.md, CONFIRMED) cross-checked against the factory
 * pad table recovered from the OneNAND image (docs/genio-padtable.md).
 *
 * Hardware summary
 * ----------------
 * One register file at phys 0x20200000:
 *
 *   offset      function                          status
 *   0x1810+     pad function grid: 22 regs x 8    CONFIRMED (factory table
 *              4-bit fields = 176 pads, codes 0-4  writes this grid)
 *   0x1970/80   2 unknown boolean regs (pull?)     INFERRED
 *   0x1990+0x10N  WAKE slot bank N (0-5):          CONFIRMED
 *              +0 status, +4 set, +8 clr
 *   0x19E0/0x1AB0  WAKE trios banks 5/6            CONFIRMED
 *   0x19F0+0x10N  DATA slot bank N (0-4):          CONFIRMED
 *              +0 input, +4 set, +8 clr
 *   0x1A50+0x10N  EDGE slot bank N (0-4):          CONFIRMED write-only,
 *              +4 clr, +8 set                      semantics open
 *   0x3400+0x40N  IRQ/direction block bank N:      CONFIRMED stores,
 *              +0x04 gate, +0x08 irq mask set,     semantics partly open
 *              +0x10 dir set, +0x14 dir clr,
 *              +0x24 pending (16-bit), +0x28/0x2c/0x34/0x38 int en
 *
 * Banks 0-4 are full GPIO banks (160 pins of the 176-pad grid; the remaining
 * 16 pads have no GPIO data path).  Banks 5-6 are wake-only (no DATIN/DIR).
 * Symbian exposes only banks 0-2 through its kernel GPIO API; USB (bank 4)
 * and MMC (bank 1) drivers touch registers directly.
 *
 * Pad numbering: grid pad = row*8 + col over the 0x1810-0x1960 window.
 * Whether grid pad N == Symbian "global pin" N (bank*32+pin) is INFERRED,
 * not yet proven; the MMC anchor (bank1 pins 26-30 = global 58-62 sitting
 * on the solid code-4 run at grid pads 57-62) supports it but is not
 * conclusive (docs/genio-padtable.md §5).
 *
 * Function codes 0-4: 0 dominates the factory table and is assumed to be
 * GPIO; 1-4 are alternate functions whose names are pending correlation
 * (candidates: eMMC/mass-memory bus, DSI, camera CSI, PMIC handshake).
 *
 * Interrupts: the bank IRQ block is implemented in hardware but the wiring
 * to a CPU interrupt controller is not yet decoded, so no irqchip is
 * registered yet.  TODO-FOR-LINUX-RUNTIME: read bank IRQ numbers once
 * bare-metal Linux boots, then implement the irqchip against the
 * +0x3400+0x40N block (see gpio-static-pinmux.md §3).
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/gpio/driver.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define BCM2727_NUM_PADS		176
#define BCM2727_NUM_GPIOS	160	/* banks 0-4 x 32 */
#define BCM2727_NUM_FUNCS	5

/* register offsets from the 0x20200000 register-file base */
#define BCM2727_MODE_BASE	0x1810	/* + (pad >> 3) * 0x10, nibble field */
#define BCM2727_WAKE_BASE	0x1990	/* + bank * 0x10; +0 stat +4 set +8 clr */
#define BCM2727_DATA_BASE	0x19F0	/* + bank * 0x10; +0 in  +4 set +8 clr */
#define BCM2727_EDGE_BASE	0x1A50	/* + bank * 0x10; +4 clr +8 set (WO) */
#define BCM2727_IRQ_BASE	0x3400	/* + bank * 0x40 */
#define BCM2727_IRQ_GATE	0x04	/* 1 on direction(out) path, 0 on in */
#define BCM2727_IRQ_MASKSET	0x08	/* write 1<<pin */
#define BCM2727_IRQ_DIRSET	0x10	/* write 1<<pin: output */
#define BCM2727_IRQ_DIRCLR	0x14	/* write 1<<pin: input */

/* bank of a global gpio number; pads and gpios share numbering */
#define BANK(gp)	((gp) >> 5)
#define PIN(gp)		((gp) & 0x1f)

struct bcm2727_pinctrl {
	struct device *dev;
	void __iomem *base;
	struct pinctrl_dev *pctl;
	struct gpio_chip gc;
	raw_spinlock_t lock;
};

static struct pinctrl_pin_desc bcm2727_pins[BCM2727_NUM_PADS];
static const char * const bcm2727_func_names[BCM2727_NUM_FUNCS] = {
	"gpio", "alt1", "alt2", "alt3", "alt4",
};

static inline void __iomem *mode_reg(struct bcm2727_pinctrl *pc, unsigned pin)
{
	return pc->base + BCM2727_MODE_BASE + (pin >> 3) * 0x10;
}

static u32 bcm2727_get_mode(struct bcm2727_pinctrl *pc, unsigned pin)
{
	u32 v = readl(mode_reg(pc, pin));

	return (v >> ((pin & 7) * 4)) & 0xf;
}

static void bcm2727_set_mode(struct bcm2727_pinctrl *pc, unsigned pin, u32 fn)
{
	unsigned shift = (pin & 7) * 4;
	unsigned long flags;
	u32 v;

	raw_spin_lock_irqsave(&pc->lock, flags);
	v = readl(mode_reg(pc, pin));
	v = (v & ~(0xf << shift)) | ((fn & 0xf) << shift);
	writel(v, mode_reg(pc, pin));
	raw_spin_unlock_irqrestore(&pc->lock, flags);
}

static int bcm2727_pctl_get_groups_count(struct pinctrl_dev *pctldev)
{
	return BCM2727_NUM_PADS;
}

static const char *bcm2727_pctl_get_group_name(struct pinctrl_dev *pctldev,
					       unsigned group)
{
	return bcm2727_pins[group].name;
}

static int bcm2727_pctl_get_group_pins(struct pinctrl_dev *pctldev,
				       unsigned group,
				       const unsigned **pins,
				       unsigned *num_pins)
{
	*pins = &bcm2727_pins[group].number;
	*num_pins = 1;
	return 0;
}

static void bcm2727_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
				 unsigned offset)
{
	struct bcm2727_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	u32 mode = bcm2727_get_mode(pc, offset);

	if (mode < BCM2727_NUM_FUNCS)
		seq_printf(s, "code %u (%s)", mode, bcm2727_func_names[mode]);
	else
		seq_printf(s, "code %u (reserved)", mode);
}

static const struct pinctrl_ops bcm2727_pctl_ops = {
	.get_groups_count = bcm2727_pctl_get_groups_count,
	.get_group_name = bcm2727_pctl_get_group_name,
	.get_group_pins = bcm2727_pctl_get_group_pins,
	.pin_dbg_show = bcm2727_pin_dbg_show,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
};

static int bcm2727_pmux_get_functions_count(struct pinctrl_dev *pctldev)
{
	return BCM2727_NUM_FUNCS;
}

static const char *bcm2727_pmux_get_function_name(struct pinctrl_dev *pctldev,
						  unsigned selector)
{
	return bcm2727_func_names[selector];
}

static int bcm2727_pmux_get_function_groups(struct pinctrl_dev *pctldev,
					    unsigned selector,
					    const char * const **groups,
					    unsigned *num_groups)
{
	/* every function is available on every pad */
	*groups = NULL;
	*num_groups = 0;
	return 0;
}

static int bcm2727_pmux_set(struct pinctrl_dev *pctldev, unsigned selector,
			    unsigned group)
{
	struct bcm2727_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	bcm2727_set_mode(pc, group, selector);
	return 0;
}

static int bcm2727_pmux_gpio_request_enable(struct pinctrl_dev *pctldev,
					    struct pinctrl_gpio_range *range,
					    unsigned offset)
{
	struct bcm2727_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	bcm2727_set_mode(pc, offset, 0);
	return 0;
}

static void bcm2727_pmux_gpio_disable_free(struct pinctrl_dev *pctldev,
					   struct pinctrl_gpio_range *range,
					   unsigned offset)
{
	/* leave the pad in whatever state it had; bootloader config wins */
}

static const struct pinmux_ops bcm2727_pmux_ops = {
	.get_functions_count = bcm2727_pmux_get_functions_count,
	.get_function_name = bcm2727_pmux_get_function_name,
	.get_function_groups = bcm2727_pmux_get_function_groups,
	.set_mux = bcm2727_pmux_set,
	.gpio_request_enable = bcm2727_pmux_gpio_request_enable,
	.gpio_disable_free = bcm2727_pmux_gpio_disable_free,
	.strict = true,
};

static int bcm2727_gpio_get_direction(struct gpio_chip *gc, unsigned offset)
{
	/*
	 * No direction readback register is known (the +0x0C readback in the
	 * IRQ block is the interrupt mask).  Report unknown.
	 */
	return -ENOTSUPP;
}

static int bcm2727_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct bcm2727_pinctrl *pc = gpiochip_get_data(gc);
	void __iomem *irqblk = pc->base + BCM2727_IRQ_BASE + BANK(offset) * 0x40;

	/* mirrors the Symbian SetPinDirection(in) sequence (CONFIRMED) */
	writel(0, irqblk + BCM2727_IRQ_GATE);
	writel(1 << PIN(offset), irqblk + BCM2727_IRQ_DIRCLR);
	return 0;
}

static int bcm2727_gpio_direction_output(struct gpio_chip *gc, unsigned offset,
					 int value)
{
	struct bcm2727_pinctrl *pc = gpiochip_get_data(gc);
	void __iomem *irqblk = pc->base + BCM2727_IRQ_BASE + BANK(offset) * 0x40;
	void __iomem *data = pc->base + BCM2727_DATA_BASE + BANK(offset) * 0x10;

	if (value)
		writel(1 << PIN(offset), data + 4);
	else
		writel(1 << PIN(offset), data + 8);

	/* mirrors the Symbian SetPinDirection(out) sequence (CONFIRMED) */
	writel(1, irqblk + BCM2727_IRQ_GATE);
	writel(1 << PIN(offset), irqblk + BCM2727_IRQ_DIRSET);
	return 0;
}

static int bcm2727_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct bcm2727_pinctrl *pc = gpiochip_get_data(gc);
	void __iomem *data = pc->base + BCM2727_DATA_BASE + BANK(offset) * 0x10;

	return (readl(data) >> PIN(offset)) & 1;
}

static void bcm2727_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct bcm2727_pinctrl *pc = gpiochip_get_data(gc);
	void __iomem *data = pc->base + BCM2727_DATA_BASE + BANK(offset) * 0x10;

	if (value)
		writel(1 << PIN(offset), data + 4);
	else
		writel(1 << PIN(offset), data + 8);
}

static int bcm2727_gpio_request(struct gpio_chip *gc, unsigned offset)
{
	return pinctrl_gpio_request(gc, offset);
}

static void bcm2727_gpio_free(struct gpio_chip *gc, unsigned offset)
{
	pinctrl_gpio_free(gc, offset);
}

static int bcm2727_probe(struct platform_device *pdev)
{
	struct bcm2727_pinctrl *pc;
	struct pinctrl_desc *pctldesc;
	int ret, i;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;
	pctldesc = devm_kzalloc(&pdev->dev, sizeof(*pctldesc), GFP_KERNEL);
	if (!pctldesc)
		return -ENOMEM;

	pc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pc->base))
		return PTR_ERR(pc->base);

	pc->dev = &pdev->dev;
	raw_spin_lock_init(&pc->lock);

	for (i = 0; i < BCM2727_NUM_PADS; i++) {
		bcm2727_pins[i].number = i;
		bcm2727_pins[i].name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						      "genio%d", i);
		if (!bcm2727_pins[i].name)
			return -ENOMEM;
	}

	pctldesc->name = dev_name(&pdev->dev);
	pctldesc->pins = bcm2727_pins;
	pctldesc->npins = BCM2727_NUM_PADS;
	pctldesc->pctlops = &bcm2727_pctl_ops;
	pctldesc->pmxops = &bcm2727_pmux_ops;
	pctldesc->owner = THIS_MODULE;

	ret = devm_pinctrl_register_and_init(&pdev->dev, pctldesc, pc, &pc->pctl);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "could not register pinctrl driver\n");

	ret = pinctrl_enable(pc->pctl);
	if (ret)
		return ret;

	pc->gc.label = "bcm2727-genio";
	pc->gc.parent = &pdev->dev;
	pc->gc.request = bcm2727_gpio_request;
	pc->gc.free = bcm2727_gpio_free;
	pc->gc.get_direction = bcm2727_gpio_get_direction;
	pc->gc.direction_input = bcm2727_gpio_direction_input;
	pc->gc.direction_output = bcm2727_gpio_direction_output;
	pc->gc.get = bcm2727_gpio_get;
	pc->gc.set = bcm2727_gpio_set;
	pc->gc.base = -1;
	pc->gc.ngpio = BCM2727_NUM_GPIOS;

	/* gpiochip adds a pin range against the pinctrl device automatically */
	ret = devm_gpiochip_add_data(&pdev->dev, &pc->gc, pc);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "could not add gpiochip\n");

	platform_set_drvdata(pdev, pc);
	dev_info(&pdev->dev,
		 "BCM2727B1 GENIO: %d pads, %d gpios (banks 0-4); banks 5/6 wake-only\n",
		 BCM2727_NUM_PADS, BCM2727_NUM_GPIOS);
	return 0;
}

static const struct of_device_id bcm2727_pinctrl_match[] = {
	{ .compatible = "brcm,bcm2727b1-genio" },
	{ }
};
MODULE_DEVICE_TABLE(of, bcm2727_pinctrl_match);

static struct platform_driver bcm2727_pinctrl_driver = {
	.driver = {
		.name = "bcm2727-pinctrl",
		.of_match_table = bcm2727_pinctrl_match,
	},
	.probe = bcm2727_probe,
};
module_platform_driver(bcm2727_pinctrl_driver);

MODULE_DESCRIPTION("Broadcom BCM2727B1 (RAPU) GENIO pinctrl/GPIO driver");
MODULE_AUTHOR("nokia-e7-linux project");
MODULE_LICENSE("GPL");
