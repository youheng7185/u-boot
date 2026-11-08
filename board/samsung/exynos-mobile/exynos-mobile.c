// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos Generic Board Source (for mobile devices)
 *
 * Copyright (c) 2025 Kaustabh Chakraborty <kauschluss@disroot.org>
 */

#include <asm/armv8/mmu.h>
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
#include <memtop.h>
#include <malloc.h>
#include <part.h>
#include <stdbool.h>
#include <string.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <linux/bitops.h>

DECLARE_GLOBAL_DATA_PTR;

#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
#define GTS9_FDT_CAPACITY SZ_1M
#define GTS9_FB_BASE 0xfa200000U
#define GTS9_FB_SIZE 0x00fa0000U
#define GTS9_DECON_BASE 0x14940000U
#define GTS9_GLOBAL_CON 0x20U
#define GTS9_TRIG_CON 0x30U
#define GTS9_COMMAND_MODE BIT(8)
#define GTS9_HW_TRIG_MASK BIT(4)
#define GTS9_HW_TRIG_EN BIT(0)
#define GTS9_FB_WIDTH 1600U
#define GTS9_UBOOT_WINDOW SZ_32M
#define GTS9_PROGRESS_Y 64U
#define GTS9_PROGRESS_W 64U
#define GTS9_PROGRESS_H 32U

void gts9_early_progress(unsigned int stage)
{
	static const u32 colors[69] = {
		0x00ffffff, 0x00ff8000, 0x0000ff00, 0x0000ffff,
		0x000000ff, 0x00ff00ff, 0x00ffff00, 0x0080ff00,
		0x000080ff, 0x00ff0080, 0x008000ff, 0x00ff8000,
		0x0000ff80, 0x0080ffff, 0x00ff8080, 0x00c0c0c0,
		0x00ffffff, 0x00ffff00, 0x0000ffff, 0x0000ff00,
		0x00ff00ff, 0x00ff8000,
		0x00ffffff, 0x0000ff00, 0x0000ffff, 0x00ffff00,
		0x00ff00ff, 0x000000ff, 0x00ff8000, 0x0080ffff,
		0x00ffffff, 0x0000ff00,
		0x0000ffff, 0x00ffff00, 0x00ff8000, 0x00ff00ff,
		0x00ff0000, 0x000000ff, 0x00c0c0c0, 0x0000ff80,
		0x008000ff,
		0x00ffffff, 0x0000ff00, 0x000000ff, 0x00ffff00,
		0x00ffffff, 0x0000ff00, 0x0000ffff, 0x00ffff00,
		0x00ff8000, 0x00ff00ff, 0x00ff0000,
		0x00ffffff, 0x0000ff00, 0x0000ffff, 0x00ffff00,
		0x00ff8000, 0x00ff00ff, 0x000000ff,
		0x00ffffff, 0x0000ff00, 0x0000ffff, 0x00ffff00,
		0x00ff8000, 0x00ff00ff,
		0x00ffffff, 0x0000ff00, 0x0000ffff, 0x00ffff00,
	};
	unsigned int x, y, col, start_y;
	const u32 color = stage && stage <= ARRAY_SIZE(colors) ?
			  colors[stage - 1] : 0;

	if (!color)
		return;
	x = 16 + (stage > 65 ? stage - 66 :
		  stage > 59 ? stage - 60 :
		  stage > 52 ? stage - 53 :
		  stage > 45 ? stage - 46 :
		  stage > 30 ? stage - 31 :
		  stage > 22 ? stage - 23 : (stage - 1) % 16) * 96;
	start_y = stage > 65 ? 448 :
		  stage > 59 ? 400 :
		  stage > 52 ? 352 :
		  stage > 45 ? 304 :
		  stage > 30 ? 256 :
		  stage > 22 ? 160 :
		  GTS9_PROGRESS_Y + ((stage - 1) / 16) * 48;
	for (y = start_y; y < start_y + GTS9_PROGRESS_H; y++) {
		volatile u32 *row = (volatile u32 *)GTS9_FB_BASE +
				y * GTS9_FB_WIDTH + x;

		for (col = 0; col < GTS9_PROGRESS_W; col++)
			row[col] = color;
		flush_dcache_range((ulong)row,
				   (ulong)(row + GTS9_PROGRESS_W));
	}
	asm volatile("dsb sy" ::: "memory");
}

static int gts9_add_framebuffer(void *fdt)
{
	fdt32_t reg[] = { cpu_to_fdt32(0), cpu_to_fdt32(GTS9_FB_BASE),
			  cpu_to_fdt32(GTS9_FB_SIZE) };
	int chosen, fb, ret;

	chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		chosen = fdt_add_subnode(fdt, 0, "chosen");
	if (chosen < 0)
		return chosen;
	ret = fdt_setprop_u32(fdt, chosen, "#address-cells", 2);
	ret = ret ?: fdt_setprop_u32(fdt, chosen, "#size-cells", 1);
	if (ret)
		return ret;

	fb = fdt_subnode_offset(fdt, chosen, "framebuffer@fa200000");
	if (fb < 0)
		fb = fdt_add_subnode(fdt, chosen, "framebuffer@fa200000");
	if (fb < 0)
		return fb;

	ret = fdt_setprop_string(fdt, fb, "compatible", "simple-framebuffer");
	ret = ret ?: fdt_setprop(fdt, fb, "reg", reg, sizeof(reg));
	ret = ret ?: fdt_setprop_u32(fdt, fb, "width", 1600);
	ret = ret ?: fdt_setprop_u32(fdt, fb, "height", 2560);
	ret = ret ?: fdt_setprop_u32(fdt, fb, "stride", 1600 * 4);
	ret = ret ?: fdt_setprop_string(fdt, fb, "format", "a8r8g8b8");
	return ret;
}

static int gts9_add_fb_serial(void *fdt)
{
	int chosen, console, ret;

	chosen = fdt_path_offset(fdt, "/chosen");
	if (chosen < 0)
		return chosen;
	console = fdt_subnode_offset(fdt, chosen, "framebuffer-console");
	if (console < 0)
		console = fdt_add_subnode(fdt, chosen, "framebuffer-console");
	if (console < 0)
		return console;

	ret = fdt_setprop_string(fdt, console, "compatible",
				 "samsung,gts9fepwifi-fb-serial");
	ret = ret ?: fdt_setprop(fdt, console, "bootph-all", NULL, 0);
	ret = ret ?: fdt_setprop_string(fdt, chosen, "stdout-path",
					  "/chosen/framebuffer-console");
	return ret;
}

static void gts9_unmask_decon_if_needed(void)
{
	u32 global = readl((void __iomem *)(GTS9_DECON_BASE + GTS9_GLOBAL_CON));

	if (global & GTS9_COMMAND_MODE) {
		u32 trig = readl((void __iomem *)(GTS9_DECON_BASE + GTS9_TRIG_CON));

		trig = (trig & ~GTS9_HW_TRIG_MASK) | GTS9_HW_TRIG_EN;
		writel(trig, (void __iomem *)(GTS9_DECON_BASE + GTS9_TRIG_CON));
		asm volatile("dsb sy" ::: "memory");
	}
}

#endif

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

static const char *exynos_prev_bl_get_bootargs(void)
{
	void *prev_bl_fdt_base = (void *)get_prev_bl_fdt_addr();
	int chosen_node_offset, ret;
	const struct fdt_property *bootargs_prop;

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

static void exynos_parse_dram_banks(const void *fdt_base)
{
	u64 mem_addr, mem_size = 0;
	u32 na, ns, i;
	int index = 1;
	int offset;

	if (fdt_check_header(fdt_base) < 0)
		return;

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

#if !IS_ENABLED(CONFIG_GTS9FEPWIFI)
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
#endif

int board_fdt_blob_setup(void **fdtp)
{
#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
	const void *stock_fdt = (const void *)get_prev_bl_fdt_addr();
	void *gts9_fdt;
	int ret;

	if (fdt_check_header(stock_fdt))
		return -EINVAL;
	/* BSS overlaps .rela.dyn before relocation. Keep this copy in the
	 * pre-relocation allocator until reserve_fdt() moves it to final RAM.
	 */
	gts9_fdt = malloc(GTS9_FDT_CAPACITY);
	if (!gts9_fdt)
		return -ENOMEM;
	ret = fdt_open_into(stock_fdt, gts9_fdt, GTS9_FDT_CAPACITY);
	if (ret)
		return ret;
	ret = fdt_add_mem_rsv(gts9_fdt, GTS9_FB_BASE, GTS9_FB_SIZE);
	if (ret)
		return ret;
	ret = gts9_add_framebuffer(gts9_fdt);
	if (ret)
		return ret;
	ret = gts9_add_fb_serial(gts9_fdt);
	if (ret)
		return ret;
	*fdtp = gts9_fdt;
	return 0;
#else
	/* If internal FDT is not available, use the external FDT instead. */
	if (fdt_check_header(*fdtp))
		*fdtp = (void *)get_prev_bl_fdt_addr();

	return 0;
#endif
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

int board_early_init_f(void)
{
#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
	gts9_unmask_decon_if_needed();
#endif
	exynos_parse_dram_banks(gd->fdt_blob);

	return 0;
}

int dram_init(void)
{
	unsigned int i;

#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
	gts9_early_progress(30);
#endif

	/* Select the largest RAM bank for U-Boot. */
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (gd->ram_size < mem_map[i + 1].size) {
			gd->ram_base = mem_map[i + 1].phys;
			gd->ram_size = mem_map[i + 1].size;
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	phys_addr_t base;

	/* Keep the image, malloc arena and stack inside one unreserved span. */
	base = get_mem_top(gd->ram_base, gd->ram_size, GTS9_UBOOT_WINDOW,
			   (void *)gd->fdt_blob);
	if (!base)
		hang();

	return base + GTS9_UBOOT_WINDOW;
}
#endif

int dram_init_banksize(void)
{
	unsigned int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		gd->dram[i].start = mem_map[i + 1].phys;
		gd->dram[i].size = mem_map[i + 1].size;
	}

	return 0;
}

int board_init(void)
{
	return 0;
}

int misc_init_r(void)
{
#if !IS_ENABLED(CONFIG_GTS9FEPWIFI)
	int ret;
#endif

	exynos_env_setup();

#if IS_ENABLED(CONFIG_GTS9FEPWIFI)
	return 0;
#else

	ret = exynos_blk_env_setup();
	if (ret)
		return ret;

	return exynos_fastboot_setup();
#endif
}
