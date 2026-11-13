// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos Generic Board Source (for mobile devices)
 *
 * Copyright (c) 2025 Kaustabh Chakraborty <kauschluss@disroot.org>
 */

#include <asm/armv8/mmu.h>
#include <asm/io.h>
#include <blk.h>
#include <bootflow.h>
#include <ctype.h>
#include <dm/ofnode.h>
#include <efi.h>
#include <efi_loader.h>
#include <env.h>
#include <errno.h>
#include <init.h>
#include <linux/sizes.h>
#include <lmb.h>
#include <part.h>
#include <stdbool.h>
#include <string.h>

DECLARE_GLOBAL_DATA_PTR;

#define lmb_alloc(size, addr) \
	lmb_alloc_mem(LMB_MEM_ALLOC_ANY, SZ_2M, addr, size, LMB_NONE)

struct efi_fw_image fw_images[] = {
	{
		.fw_name = u"UBOOT_BOOT_PARTITION",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	.dfu_string = NULL,
	.images = fw_images,
	.num_images = ARRAY_SIZE(fw_images),
};

/*
 * The memory mapping includes all DRAM banks, along with the
 * peripheral block, and a sentinel at the end. This is filled in
 * dynamically.
 */
static struct mm_region exynos_mem_map[CONFIG_NR_DRAM_BANKS + 2] = {
	{
		/* Peripheral MMIO block */
		.virt = 0x10000000UL,
		.phys = 0x10000000UL,
		.size = 0x10000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
};

struct mm_region *mem_map = exynos_mem_map;


/*
 * O1S_DBG: mirror every console byte into the ramoops "pmsg" zone
 * (0x8f020000, 128 KiB, persistent_ram_buffer layout, sig "DBGC"), so that
 * Linux (TWRP/Android) exposes it as /sys/fs/pstore/pmsg-ramoops-0.
 */
#define O1S_LOG_BASE	0x8f020000UL
#define O1S_LOG_SIZE	0x20000UL
#define O1S_LOG_SIG	0x43474244
#define O1S_CL		64UL
static int o1s_log_ready __section(".data");

static void o1s_log_flush(ulong a, ulong b)
{
	a &= ~(O1S_CL - 1);
	b = (b + O1S_CL - 1) & ~(O1S_CL - 1);
	flush_dcache_range(a, b);
}

void o1s_log_putc(char c)
{
	volatile u32 *hdr = (volatile u32 *)O1S_LOG_BASE;
	u8 *data = (u8 *)(O1S_LOG_BASE + 12);
	u32 n;

	if (!o1s_log_ready) {
		hdr[0] = O1S_LOG_SIG;
		hdr[1] = 0;
		hdr[2] = 0;
		o1s_log_ready = 1;
	}
	n = hdr[2];
	if (n >= O1S_LOG_SIZE - 12)
		return;
	data[n] = c;
	hdr[2] = n + 1;
	hdr[1] = n + 1;
	o1s_log_flush(O1S_LOG_BASE, O1S_LOG_BASE + 12);
	o1s_log_flush((ulong)&data[n], (ulong)&data[n] + 1);
}

void o1s_log_str(const char *str)
{
	while (*str)
		o1s_log_putc(*str++);
}

/* O1S_DBG: paint white band n on the bootloader framebuffer (bring-up aid) */
#include <cpu_func.h>
static void __maybe_unused o1s_dbg_band(int n)
{
	u32 *p = (u32 *)(0xfa000000UL + (150UL + n * 130UL) * 1080 * 4);
	unsigned long i;

	writel(0x1281, 0x19f00030);
	for (i = 0; i < 1080 * 100; i++)
		p[i] = 0xffffffff;
	flush_dcache_range((ulong)p, (ulong)(p + 1080 * 100));
}

/*
 * exynos2100: offset inside the boot.img "kernel" blob at which the build
 * script appends the FIT (Linux + DTB + initramfs). It must stay clear of
 * U-Boot's own image, BSS and early stack.
 */
#define EXYNOS_PAYLOAD_OFFSET		SZ_4M

/* Where the previous bootloader loaded U-Boot (recorded before relocation) */
static ulong exynos_load_addr __section(".data");

#define EXYNOS2100_WDT_CL0_WTCON	0x10050000
#define EXYNOS2100_DECON0_TRIG_CON	0x19f00030
#define EXYNOS2100_PMU_SYSIP_DAT0	0x15860810
#define EXYNOS2100_PMU_PANIC_INFORM	0x1586080c
#define EXYNOS2100_FB_BASE		0xfa000000UL
#define EXYNOS2100_FB_SIZE		0x01000000UL

/* Return the previous bootloader's FDT, or NULL if it is not usable. */
static void *exynos_prev_bl_fdt(void)
{
	ulong addr = get_prev_bl_fdt_addr();

	/* All Exynos ARMv8 SoCs supported here have DRAM at >= 1 GiB */
	if (addr < 0x40000000UL || (addr & 0x3))
		return NULL;
	if (fdt_check_header((void *)addr))
		return NULL;

	return (void *)addr;
}

static void exynos2100_quirks(void)
{
	/*
	 * Bring-up: clear the reboot mode left by "adb reboot recovery"
	 * (PMU SYSIP_DAT0 = 0xff) and the Samsung reset reason, so that the
	 * next reset (panic, watchdog, forced reboot) boots Android, which
	 * picks up our pstore log.
	 */
	writel(0, EXYNOS2100_PMU_SYSIP_DAT0);
	writel(0x12345670, EXYNOS2100_PMU_PANIC_INFORM);	/* SEC reason: normal */

	/*
	 * Bring-up: leave S-LK's cluster0 watchdog armed so a hung U-Boot
	 * resets by itself.
	 */

	/*
	 * Command-mode panel: keep DECON pushing the bootloader framebuffer
	 * to the display so that our writes become visible.
	 */
	writel(0x1281, EXYNOS2100_DECON0_TRIG_CON);
}

/* Publish the FIT appended after U-Boot (see EXYNOS_PAYLOAD_OFFSET). */
static void exynos_payload_setup(void)
{
	phys_addr_t addr;

	/* PIE build: TEXT_BASE is 0, so use the address S-Boot loaded us at */
	if (!exynos_load_addr)
		return;
	addr = exynos_load_addr + EXYNOS_PAYLOAD_OFFSET;
	void *fit = (void *)addr;
	ulong size;

	if (fdt_check_header(fit) || fdt_path_offset(fit, "/images") < 0)
		return;

	size = fdt_totalsize(fit);
	if (lmb_alloc_mem(LMB_MEM_ALLOC_ADDR, 0, &addr, size, LMB_NONE))
		log_warning("%s: could not reserve payload region\n", __func__);

	env_set_hex("payload_addr", addr);
	env_set_hex("payload_size", size);
}

/*
 * Relocate below the lowest firmware-owned region that lies above U-Boot's
 * current image inside the chosen DRAM bank (secure monitor, hypervisor,
 * debug snapshot, ramoops, ...). The previous bootloader's FDT is the
 * authority here; the built-in device tree is only a fallback.
 */
static phys_addr_t exynos_rsv_top(const void *fdt, phys_addr_t lo,
				  phys_addr_t top)
{
	int parent, node;

	if (!fdt)
		return top;

	parent = fdt_path_offset(fdt, "/reserved-memory");
	if (parent < 0)
		return top;

	fdt_for_each_subnode(node, fdt, parent) {
		fdt_addr_t addr;
		fdt_size_t size;

		addr = fdtdec_get_addr_size_auto_parent(fdt, parent, node,
							"reg", 0, &size, false);
		if (addr == FDT_ADDR_T_NONE || !size)
			continue;
		/* A region straddling U-Boot itself cannot be avoided here */
		if (addr + size <= lo)
			continue;
		if (addr < top)
			top = addr > lo ? addr : lo;
	}

	return top;
}

phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	o1s_dbg_band(6);
	phys_addr_t self = (phys_addr_t)(ulong)board_get_usable_ram_top;
	phys_addr_t top = gd->ram_base + gd->ram_size;

	top = exynos_rsv_top(exynos_prev_bl_fdt(), self, top);
	top = exynos_rsv_top(gd->fdt_blob, self, top);

	return top;
}

static const char *exynos_prev_bl_get_bootargs(void)
{
	void *prev_bl_fdt_base = exynos_prev_bl_fdt();
	int chosen_node_offset, ret;
	const struct fdt_property *bootargs_prop;

	if (!prev_bl_fdt_base)
		return NULL;

	ret = fdt_check_header(prev_bl_fdt_base);
	if (ret < 0) {
		log_err("%s: FDT is invalid (FDT_ERR %d)\n", __func__, ret);
		return NULL;
	}

	ret = fdt_path_offset(prev_bl_fdt_base, "/chosen");
	chosen_node_offset = ret;
	if (ret < 0) {
		log_err("%s: /chosen node not found (FDT_ERR %d)\n", __func__,
			ret);
		return NULL;
	}

	bootargs_prop = fdt_get_property(prev_bl_fdt_base, chosen_node_offset,
					 "bootargs", &ret);
	if (!bootargs_prop) {
		log_err("%s: /chosen/bootargs property not found (FDT_ERR %d)\n",
			__func__, ret);
		return NULL;
	}

	return bootargs_prop->data;
}

static int exynos_parse_dram_banks(const void *fdt_base)
{
	u64 mem_addr, mem_size = 0;
	u32 na, ns, i;
	int index = 1;
	int offset;

	if (!fdt_base || fdt_check_header(fdt_base) < 0)
		return 0;

	/* #address-cells and #size-cells as defined in the fdt root. */
	na = fdt_address_cells(fdt_base, 0);
	ns = fdt_size_cells(fdt_base, 0);

	fdt_for_each_subnode(offset, fdt_base, 0) {
		if (strncmp(fdt_get_name(fdt_base, offset, NULL), "memory", 6))
			continue;

		for (i = 0; ; i++) {
			if (index > CONFIG_NR_DRAM_BANKS)
				break;

			mem_addr = fdtdec_get_addr_size_fixed(fdt_base, offset,
							      "reg", i, na, ns,
							      &mem_size, false);
			if (mem_addr == FDT_ADDR_T_NONE)
				break;

			if (!mem_size)
				continue;

			mem_map[index].phys = mem_addr;
			mem_map[index].virt = mem_addr;
			mem_map[index].size = mem_size;
			mem_map[index].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					       PTE_BLOCK_INNER_SHARE;
			index++;
		}
	}

	return index - 1;
}

static void exynos_env_setup(void)
{
	const char *bootargs = exynos_prev_bl_get_bootargs();
	const char *dev_compatible, *soc_compatible;
	char *ptr;
	char buf[128];
	int nr_compatibles;
	int offset;
	int ret;

	if (bootargs) {
		/* Read the cmdline property which stores the serial number. */
		ret = cmdline_get_arg(bootargs, "androidboot.serialno", &offset);
		if (ret > 0) {
			strlcpy(buf, bootargs + offset, ret);
			env_set("serial#", buf);
		}
	}

	nr_compatibles = ofnode_read_string_count(ofnode_root(), "compatible");
	if (nr_compatibles < 2) {
		log_warning("%s: expected 2 or more compatible strings\n",
			    __func__);
		return;
	}

	ret = ofnode_read_string_index(ofnode_root(), "compatible",
				       nr_compatibles - 1, &soc_compatible);
	if (ret) {
		log_warning("%s: failed to read SoC compatible\n",
			    __func__);
		return;
	}

	ret = ofnode_read_string_index(ofnode_root(), "compatible", 0,
				       &dev_compatible);
	if (ret) {
		log_warning("%s: failed to read device compatible\n",
			    __func__);
		return;
	}

	/* <manufacturer>,<soc> => platform = <soc> */
	ptr = strchr(soc_compatible, ',');
	if (ptr)
		soc_compatible = ptr + 1;
	env_set("platform", soc_compatible);

	/* <manufacturer>,<codename> => board = <manufacturer>-<codename> */
	strlcpy(buf, dev_compatible, sizeof(buf) - 1);
	ptr = strchr(buf, ',');
	if (ptr)
		*ptr = '-';
	env_set("board", buf);

	/*
	 * NOTE: Board name usually goes as <manufacturer>-<codename>, but
	 * upstream device trees for Exynos SoCs are <soc>-<codename>.
	 * Extraction of <codename> from the board name is required.
	 */
	ptr = strchr(dev_compatible, ',');
	if (ptr)
		dev_compatible = ptr + 1;

	/* EFI booting requires the path to correct DTB, specify it here. */
	snprintf(buf, sizeof(buf), "exynos/%s-%s.dtb", soc_compatible,
		 dev_compatible);
	env_set("fdtfile", buf);
}

static int exynos_blk_env_setup(void)
{
	const char *blk_ifname;
	int blk_dev = 0;
	struct blk_desc *blk_desc;
	struct disk_partition info = {0};
	unsigned long largest_part_start = 0, largest_part_size = 0;
	static char dfu_string[32];
	int i;

	blk_ifname = "mmc";
	blk_desc = blk_get_dev(blk_ifname, blk_dev);
	if (!blk_desc) {
		log_err("%s: required mmc device not available\n", __func__);
		return -ENODEV;
	}

	for (i = 1; i < CONFIG_EFI_PARTITION_ENTRIES_NUMBERS; i++) {
		if (part_get_info(blk_desc, i, &info))
			continue;

		if (!update_info.dfu_string &&
		    !strncasecmp(info.name, "boot", strlen("boot"))) {
			snprintf(dfu_string, sizeof(dfu_string),
				 "mmc %d=u-boot.bin part %d %d", blk_dev,
				 blk_dev, i);
			update_info.dfu_string = dfu_string;
		}

		if (info.start > largest_part_size) {
			largest_part_start = info.start;
			largest_part_size = info.size;
		}
	}

	if (largest_part_size) {
		env_set("blkmap_blk_ifname", blk_ifname);
		env_set_ulong("blkmap_blk_dev", blk_dev);
		env_set_ulong("blkmap_blk_nr", largest_part_start);
		env_set_hex("blkmap_size_r", largest_part_size);
	} else {
		log_warning("%s: no qualified partition for blkmap, skipping\n",
			    __func__);
	}

	return 0;
}

static int exynos_fastboot_setup(void)
{
	struct blk_desc *blk_dev;
	struct disk_partition info = {0};
	char buf[128];
	phys_addr_t addr;
	int offset, i, j;

	/* Allocate and define buffer address for fastboot interface. */
	if (lmb_alloc(CONFIG_FASTBOOT_BUF_SIZE, &addr)) {
		log_err("%s: failed to allocate fastboot buffer\n", __func__);
		return -ENOMEM;
	}
	env_set_hex("fastboot_addr_r", addr);

	blk_dev = blk_get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!blk_dev) {
		log_err("%s: required mmc device not available\n", __func__);
		return -ENODEV;
	}

	strcpy(buf, "fastboot_partition_alias_");
	offset = strlen(buf);

	for (i = 1; i < CONFIG_EFI_PARTITION_ENTRIES_NUMBERS; i++) {
		if (part_get_info(blk_dev, i, &info))
			continue;

		/*
		 * The partition name must be lowercase (stored in buf[]),
		 * as is expected in all fastboot partitions ...
		 */
		strlcpy(buf + offset, info.name, sizeof(buf) - offset);
		for (j = offset; buf[j]; j++)
			buf[j] = tolower(buf[j]);
		if (!strcmp(buf + offset, info.name))
			continue;
		/*
		 * ... However, if that isn't the case, a fastboot
		 * partition alias must be defined to establish it.
		 */
		env_set(buf, info.name);
	}

	return 0;
}

int board_fdt_blob_setup(void **fdtp)
{
	o1s_dbg_band(1);
	/* If internal FDT is not available, use the external FDT instead. */
	if (fdt_check_header(*fdtp))
		*fdtp = exynos_prev_bl_fdt();

	return 0;
}

int timer_init(void)
{
	ofnode timer_node;

	/*
	 * In a lot of Exynos devices, the previous bootloader does not
	 * set CNTFRQ_EL0 properly. However, the timer node in
	 * devicetree has the correct frequency, use that instead.
	 */
	timer_node = ofnode_by_compatible(ofnode_null(), "arm,armv8-timer");
	gd->arch.timer_rate_hz = ofnode_read_u32_default(timer_node,
							 "clock-frequency", 0);

	return 0;
}


/*
 * exynos2100 bring-up: keep only the DRAM bank U-Boot runs from. The other
 * banks contain firmware/hypervisor memory (e.g. Samsung uH "el2_earlymem"
 * at 0x9fec00000) that is only described in the previous bootloader's
 * reserved-memory, which LMB/EFI do not honour yet; allocating there hangs.
 */
static void exynos_keep_own_bank(void)
{
	ulong self = (ulong)exynos_keep_own_bank;
	int i;

	for (i = 1; i <= CONFIG_NR_DRAM_BANKS; i++) {
		if (mem_map[i].size && self >= mem_map[i].phys &&
		    self - mem_map[i].phys < mem_map[i].size)
			break;
	}
	if (i > CONFIG_NR_DRAM_BANKS)
		return;

	mem_map[1] = mem_map[i];
	for (i = 2; i <= CONFIG_NR_DRAM_BANKS; i++)
		mem_map[i].size = 0;

	/* Keep the bootloader framebuffer mapped; it is not a DRAM bank. */
	mem_map[2].phys = EXYNOS2100_FB_BASE;
	mem_map[2].virt = EXYNOS2100_FB_BASE;
	mem_map[2].size = EXYNOS2100_FB_SIZE;
	mem_map[2].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE;
}

int board_early_init_f(void)
{
	extern char __image_copy_start[];

	exynos_load_addr = (ulong)__image_copy_start;
	o1s_dbg_band(2);
	if (!fdt_node_check_compatible(gd->fdt_blob, 0,
				       "samsung,exynos2100"))
		exynos2100_quirks();
	o1s_dbg_band(3);

	/*
	 * Trust the memory layout handed over by the previous
	 * bootloader; fall back to the built-in device tree.
	 */
	if (!exynos_parse_dram_banks(exynos_prev_bl_fdt()))
		exynos_parse_dram_banks(gd->fdt_blob);
	if (!fdt_node_check_compatible(gd->fdt_blob, 0, "samsung,exynos2100"))
		exynos_keep_own_bank();
	o1s_dbg_band(4);

	return 0;
}

int dram_init(void)
{
	ulong self = (ulong)dram_init;
	unsigned int i;

	o1s_dbg_band(5);

	/* Prefer the RAM bank U-Boot is currently running from. */
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (mem_map[i + 1].size &&
		    self >= mem_map[i + 1].phys &&
		    self - mem_map[i + 1].phys < mem_map[i + 1].size) {
			gd->ram_base = mem_map[i + 1].phys;
			gd->ram_size = mem_map[i + 1].size;
			return 0;
		}
	}

	/* Otherwise select the largest RAM bank for U-Boot. */
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (gd->ram_size < mem_map[i + 1].size) {
			gd->ram_base = mem_map[i + 1].phys;
			gd->ram_size = mem_map[i + 1].size;
		}
	}

	return 0;
}

int dram_init_banksize(void)
{
	unsigned int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		gd->dram[i].start = mem_map[i + 1].phys;
		gd->dram[i].size = mem_map[i + 1].size;
		/* The framebuffer window is mapped but is not usable RAM */
		if (mem_map[i + 1].phys == EXYNOS2100_FB_BASE)
			gd->dram[i].size = 0;
	}

	return 0;
}


/* O1S_DBG: thin ticks after each init_sequence_r step from board_init on */
int o1s_after_board_init;
void o1s_dbg_tick(int failed)
{
	static int n;
	u32 *p, col;
	unsigned long i;

	if (!o1s_after_board_init)
		return;
	if (failed) {
		p = (u32 *)(0xfa000000UL + 2300UL * 1080 * 4);
		for (i = 0; i < 1080 * 100; i++)
			p[i] = 0xffff0000;
		flush_dcache_range((ulong)p, (ulong)(p + 1080 * 100));
	}
	col = (n % 5 == 4) ? 0xffffff00 : 0xffffffff;
	p = (u32 *)(0xfa000000UL + (1450UL + n * 32UL) * 1080 * 4);
	for (i = 0; i < 1080 * 22; i++)
		p[i] = col;
	flush_dcache_range((ulong)p, (ulong)(p + 1080 * 22));
	n++;
}

int board_init(void)
{
	o1s_dbg_band(7);
	o1s_after_board_init = 1;
	return 0;
}

int misc_init_r(void)
{
	o1s_dbg_band(8);
	exynos_env_setup();
	exynos_payload_setup();

	/*
	 * Storage is optional: SoCs without a block driver yet (exynos2100)
	 * boot from the payload embedded in the boot image.
	 */
	if (exynos_blk_env_setup())
		return 0;

	exynos_fastboot_setup();
	o1s_dbg_band(9);

	return 0;
}
