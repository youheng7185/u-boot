// SPDX-License-Identifier: GPL-2.0+
/* Minimal visual marker proving execution reached U-Boot board C init. */

#include <cpu_func.h>
#include <init.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/types.h>

#define J2_FB_BASE     0xA800035AUL
#define J2_FB_STRIDE   1620
#define J2_MARKER_X    16
#define J2_MARKER_Y    16
#define J2_BLOCK_W     120
#define J2_BLOCK_H     48
#define J2_PROGRESS_Y  128
#define J2_PROGRESS_W  24
#define J2_PROGRESS_H  24

/* Dedicated MSM8917 SDC2 pad control; this does not overlap SDC1/eMMC. */
#define J2_TLMM_SDC2_CFG 0x01109000UL
#define J2_SDC2_CLK_PULL GENMASK(15, 14)
#define J2_SDC2_CLK_DRV  GENMASK(8, 6)
#define J2_SDC2_CMD_PULL GENMASK(12, 11)
#define J2_SDC2_CMD_DRV  GENMASK(5, 3)
#define J2_SDC2_DAT_PULL GENMASK(10, 9)
#define J2_SDC2_DAT_DRV  GENMASK(2, 0)

DECLARE_GLOBAL_DATA_PTR;

/* Relocate below the modem carveout while retaining the full SMEM RAM map. */
#define J2_UBOOT_RELOC_TOP 0x93000000UL

phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	(void)total_size;

	return gd->ram_top < J2_UBOOT_RELOC_TOP ? gd->ram_top :
		J2_UBOOT_RELOC_TOP;
}

void j2_early_progress(unsigned int stage)
{
	u8 *fb = (u8 *)J2_FB_BASE;
	unsigned int x, row, col;

	if (!stage || stage > 12)
		return;
	x = 16 + (stage - 1) * 32;
	for (row = 0; row < J2_PROGRESS_H; row++) {
		u8 *pixel = fb + (J2_PROGRESS_Y + row) * J2_FB_STRIDE + x * 3;

		for (col = 0; col < J2_PROGRESS_W; col++) {
			*pixel++ = 0x00;
			*pixel++ = 0xff;
			*pixel++ = 0x00;
		}
	}
	flush_dcache_range(J2_FB_BASE + J2_PROGRESS_Y * J2_FB_STRIDE + x * 3,
			   J2_FB_BASE + (J2_PROGRESS_Y + J2_PROGRESS_H) *
			   J2_FB_STRIDE + (x + J2_PROGRESS_W) * 3);
}

static void j2_fb_block(unsigned int x, u8 red, u8 green, u8 blue)
{
	u8 *fb = (u8 *)J2_FB_BASE;
	unsigned int row, col;

	for (row = 0; row < J2_BLOCK_H; row++) {
		u8 *pixel = fb + (J2_MARKER_Y + row) * J2_FB_STRIDE + x * 3;

		for (col = 0; col < J2_BLOCK_W; col++) {
			*pixel++ = red;
			*pixel++ = green;
			*pixel++ = blue;
		}
	}
}

int board_early_init_f(void)
{
	/* Match the kernel's active SDC2 pad settings: CLK 12mA, CMD/DATA 8mA. */
	clrsetbits_le32((void *)J2_TLMM_SDC2_CFG,
			J2_SDC2_CLK_PULL | J2_SDC2_CLK_DRV |
			J2_SDC2_CMD_PULL | J2_SDC2_CMD_DRV |
			J2_SDC2_DAT_PULL | J2_SDC2_DAT_DRV,
			(5 << 6) | (3 << 11) | (3 << 3) | (3 << 9) | 3);

	/* RGB888 blocks: red, green, blue, and white. */
	j2_fb_block(J2_MARKER_X, 0xff, 0x00, 0x00);
	j2_fb_block(J2_MARKER_X + J2_BLOCK_W, 0x00, 0xff, 0x00);
	j2_fb_block(J2_MARKER_X + 2 * J2_BLOCK_W, 0x00, 0x00, 0xff);
	j2_fb_block(J2_MARKER_X + 3 * J2_BLOCK_W, 0xff, 0xff, 0xff);
	flush_dcache_range(J2_FB_BASE + J2_MARKER_Y * J2_FB_STRIDE,
			   J2_FB_BASE + (J2_MARKER_Y + J2_BLOCK_H) *
			   J2_FB_STRIDE);

	return 0;
}
