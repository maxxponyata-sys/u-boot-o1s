// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos 2100 (S5E9840) pinctrl driver.
 *
 * Bank layout taken from the vendor kernel
 * (drivers/pinctrl/samsung/pinctrl-exynos-arm64.c, non-EVT0 silicon).
 * The controller instance is selected through the "pinctrlN" alias.
 */

#include <dm.h>
#include <dm/pinctrl.h>
#include "pinctrl-exynos.h"

#define EXYNOS2100_PIN_BANK(pins, reg, id)		\
	{						\
		.type		= &exynos2100_bank_type,\
		.offset		= reg,			\
		.nr_pins	= pins,			\
		.name		= id			\
	}

/* CON, DAT, PUD, DRV */
static const struct samsung_pin_bank_type exynos2100_bank_type = {
	.fld_width = { 4, 1, 4, 4, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, },
};

static const struct pinctrl_ops exynos2100_pinctrl_ops = {
	.set_state = exynos_pinctrl_set_state
};

/* 0: ALIVE (0x15850000) */
static const struct samsung_pin_bank_data exynos2100_pin_alive[] = {
	EXYNOS2100_PIN_BANK(4, 0x000, "gpq0"),
	EXYNOS2100_PIN_BANK(3, 0x020, "gpq1"),
	EXYNOS2100_PIN_BANK(2, 0x040, "gpq2"),
	EXYNOS2100_PIN_BANK(4, 0x060, "gpb5"),
};

/* 1: CMGP (0x14e30000) */
static const struct samsung_pin_bank_data exynos2100_pin_cmgp[] = {
	EXYNOS2100_PIN_BANK(2, 0x000, "gpm0"),
	EXYNOS2100_PIN_BANK(2, 0x020, "gpm1"),
	EXYNOS2100_PIN_BANK(2, 0x040, "gpm2"),
	EXYNOS2100_PIN_BANK(2, 0x060, "gpm3"),
	EXYNOS2100_PIN_BANK(2, 0x080, "gpm4"),
	EXYNOS2100_PIN_BANK(2, 0x0a0, "gpm5"),
	EXYNOS2100_PIN_BANK(2, 0x0c0, "gpm6"),
	EXYNOS2100_PIN_BANK(2, 0x0e0, "gpm7"),
	EXYNOS2100_PIN_BANK(1, 0x100, "gpm8"),
	EXYNOS2100_PIN_BANK(1, 0x120, "gpm9"),
	EXYNOS2100_PIN_BANK(1, 0x140, "gpm10"),
	EXYNOS2100_PIN_BANK(1, 0x160, "gpm11"),
	EXYNOS2100_PIN_BANK(1, 0x180, "gpm12"),
	EXYNOS2100_PIN_BANK(1, 0x1a0, "gpm13"),
	EXYNOS2100_PIN_BANK(1, 0x1c0, "gpm14"),
	EXYNOS2100_PIN_BANK(1, 0x1e0, "gpm15"),
	EXYNOS2100_PIN_BANK(1, 0x200, "gpm16"),
	EXYNOS2100_PIN_BANK(1, 0x220, "gpm17"),
	EXYNOS2100_PIN_BANK(1, 0x240, "gpm18"),
	EXYNOS2100_PIN_BANK(1, 0x260, "gpm19"),
	EXYNOS2100_PIN_BANK(1, 0x280, "gpm20"),
	EXYNOS2100_PIN_BANK(1, 0x2a0, "gpm21"),
	EXYNOS2100_PIN_BANK(1, 0x2c0, "gpm22"),
	EXYNOS2100_PIN_BANK(1, 0x2e0, "gpm23"),
	EXYNOS2100_PIN_BANK(1, 0x300, "gpm24"),
	EXYNOS2100_PIN_BANK(1, 0x320, "gpm25"),
};

/* 2: CMGPEINT (0x14ec0000) */
static const struct samsung_pin_bank_data exynos2100_pin_cmgpeint[] = {
	EXYNOS2100_PIN_BANK(8, 0x000, "gpa0"),
	EXYNOS2100_PIN_BANK(8, 0x020, "gpa1"),
	EXYNOS2100_PIN_BANK(8, 0x040, "gpa2"),
	EXYNOS2100_PIN_BANK(8, 0x060, "gpa3"),
	EXYNOS2100_PIN_BANK(2, 0x080, "gpa4"),
};

/* 3: HSI1 (0x11040000) */
static const struct samsung_pin_bank_data exynos2100_pin_hsi1[] = {
	EXYNOS2100_PIN_BANK(4, 0x000, "gpf0"),
	EXYNOS2100_PIN_BANK(6, 0x020, "gpf1"),
	EXYNOS2100_PIN_BANK(3, 0x040, "gpf2"),
};

/* 4: PERIC0 (0x10430000) */
static const struct samsung_pin_bank_data exynos2100_pin_peric0[] = {
	EXYNOS2100_PIN_BANK(4, 0x000, "gpp0"),
	EXYNOS2100_PIN_BANK(4, 0x020, "gpp1"),
	EXYNOS2100_PIN_BANK(4, 0x040, "gpp2"),
	EXYNOS2100_PIN_BANK(4, 0x060, "gpp3"),
	EXYNOS2100_PIN_BANK(4, 0x080, "gpp4"),
	EXYNOS2100_PIN_BANK(4, 0x0a0, "gpp5"),
	EXYNOS2100_PIN_BANK(4, 0x0c0, "gpp13"),
	EXYNOS2100_PIN_BANK(4, 0x0e0, "gpp14"),
	EXYNOS2100_PIN_BANK(4, 0x100, "gpp15"),
	EXYNOS2100_PIN_BANK(6, 0x120, "gpg0"),
};

/* 5: PERIC1 (0x10730000) */
static const struct samsung_pin_bank_data exynos2100_pin_peric1[] = {
	EXYNOS2100_PIN_BANK(4, 0x000, "gpb3"),
	EXYNOS2100_PIN_BANK(4, 0x020, "gpb4"),
	EXYNOS2100_PIN_BANK(4, 0x040, "gpp11"),
	EXYNOS2100_PIN_BANK(4, 0x060, "gpp12"),
	EXYNOS2100_PIN_BANK(4, 0x080, "gpp16"),
	EXYNOS2100_PIN_BANK(4, 0x0a0, "gpp17"),
	EXYNOS2100_PIN_BANK(4, 0x0c0, "gpp18"),
	EXYNOS2100_PIN_BANK(6, 0x0e0, "gpg1"),
	EXYNOS2100_PIN_BANK(4, 0x100, "gpg2"),
	EXYNOS2100_PIN_BANK(4, 0x120, "gpb0"),
	EXYNOS2100_PIN_BANK(4, 0x140, "gpb1"),
	EXYNOS2100_PIN_BANK(4, 0x160, "gpb2"),
};

/* 6: PERIC2 (0x11d30000) */
static const struct samsung_pin_bank_data exynos2100_pin_peric2[] = {
	EXYNOS2100_PIN_BANK(4, 0x000, "gpp6"),
	EXYNOS2100_PIN_BANK(4, 0x020, "gpp7"),
	EXYNOS2100_PIN_BANK(4, 0x040, "gpp8"),
	EXYNOS2100_PIN_BANK(4, 0x060, "gpp9"),
	EXYNOS2100_PIN_BANK(4, 0x080, "gpp10"),
};

/* 7: VTS (0x15580000) */
static const struct samsung_pin_bank_data exynos2100_pin_vts[] = {
	EXYNOS2100_PIN_BANK(7, 0x000, "gpv0"),
};

#define EXYNOS2100_CTRL(banks) \
	{ .pin_banks = banks, .nr_banks = ARRAY_SIZE(banks), }

static const struct samsung_pin_ctrl exynos2100_pin_ctrl[] = {
	EXYNOS2100_CTRL(exynos2100_pin_alive),
	EXYNOS2100_CTRL(exynos2100_pin_cmgp),
	EXYNOS2100_CTRL(exynos2100_pin_cmgpeint),
	EXYNOS2100_CTRL(exynos2100_pin_hsi1),
	EXYNOS2100_CTRL(exynos2100_pin_peric0),
	EXYNOS2100_CTRL(exynos2100_pin_peric1),
	EXYNOS2100_CTRL(exynos2100_pin_peric2),
	EXYNOS2100_CTRL(exynos2100_pin_vts),
	{/* list terminator */}
};

static const struct udevice_id exynos2100_pinctrl_ids[] = {
	{ .compatible = "samsung,exynos2100-pinctrl",
		.data = (ulong)exynos2100_pin_ctrl },
	{ }
};

U_BOOT_DRIVER(pinctrl_exynos2100) = {
	.name		= "pinctrl_exynos2100",
	.id		= UCLASS_PINCTRL,
	.of_match	= exynos2100_pinctrl_ids,
	.priv_auto	= sizeof(struct exynos_pinctrl_priv),
	.ops		= &exynos2100_pinctrl_ops,
	.probe		= exynos_pinctrl_probe,
	.bind		= exynos_pinctrl_bind,
};
