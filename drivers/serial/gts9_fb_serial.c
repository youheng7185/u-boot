// SPDX-License-Identifier: GPL-2.0+
/* Console output to the ARGB8888 framebuffer left active by Samsung S-Boot. */
#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <video_font_8x16.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <linux/string.h>

DECLARE_GLOBAL_DATA_PTR;

#define FB_ADDR         0xfa200000UL
#define FB_WIDTH        1600U
#define FB_HEIGHT       2560U
#define FB_STRIDE       (FB_WIDTH * 4U)
#define TEXT_TOP        640U
#define FONT_WIDTH      8U
#define FONT_HEIGHT     16U
#define SCALE           2U
#define CHAR_WIDTH      (FONT_WIDTH * SCALE)
#define CHAR_HEIGHT     (FONT_HEIGHT * SCALE)
#define TEXT_COLS       (FB_WIDTH / CHAR_WIDTH)
#define TEXT_ROWS       ((FB_HEIGHT - TEXT_TOP) / CHAR_HEIGHT)
#define TEXT_ADDR       (FB_ADDR + TEXT_TOP * FB_STRIDE)
#define TEXT_SIZE       ((FB_HEIGHT - TEXT_TOP) * FB_STRIDE)

struct gts9_fb_priv {
	unsigned int col;
	unsigned int row;
};

static void draw_char(unsigned int col, unsigned int row, unsigned char ch)
{
	const unsigned char *glyph = video_fontdata_8x16 + ch * FONT_HEIGHT;
	unsigned int x, y, scale_y;
	ulong first = TEXT_ADDR + row * CHAR_HEIGHT * FB_STRIDE +
		      col * CHAR_WIDTH * 4;

	for (y = 0; y < FONT_HEIGHT; y++) {
		for (scale_y = 0; scale_y < SCALE; scale_y++) {
			volatile u32 *pixel = (volatile u32 *)(first +
				(y * SCALE + scale_y) * FB_STRIDE);
			unsigned int bits = glyph[y];

			for (x = 0; x < FONT_WIDTH; x++) {
				u32 colour = (bits & (0x80 >> x)) ?
					     0x00ffffff : 0x00000000;

				pixel[x * SCALE] = colour;
				pixel[x * SCALE + 1] = colour;
			}
			flush_dcache_range((ulong)pixel,
					   (ulong)(pixel + CHAR_WIDTH));
		}
	}
	asm volatile("dsb sy" ::: "memory");
}

static void scroll_up(void)
{
	void *fb = (void *)TEXT_ADDR;
	ulong move_size = TEXT_SIZE - CHAR_HEIGHT * FB_STRIDE;

	memmove(fb, (u8 *)fb + CHAR_HEIGHT * FB_STRIDE, move_size);
	memset((u8 *)fb + move_size, 0, CHAR_HEIGHT * FB_STRIDE);
	flush_dcache_range(TEXT_ADDR, TEXT_ADDR + TEXT_SIZE);
	asm volatile("dsb sy" ::: "memory");
}

static void newline(struct gts9_fb_priv *priv)
{
	priv->col = 0;
	if (++priv->row >= TEXT_ROWS) {
		scroll_up();
		priv->row = TEXT_ROWS - 1;
	}
}

static int gts9_fb_putc(struct udevice *dev, const char ch)
{
	struct gts9_fb_priv *priv = dev_get_priv(dev);
	unsigned char c = ch;

	if (c == '\r') {
		priv->col = 0;
	} else if (c == '\n') {
		newline(priv);
	} else if (c == '\b') {
		if (priv->col)
			priv->col--;
		draw_char(priv->col, priv->row, ' ');
	} else if (c == '\t') {
		priv->col = (priv->col + 8) & ~7U;
		if (priv->col >= TEXT_COLS)
			newline(priv);
	} else if (c >= 0x20) {
		if (priv->col >= TEXT_COLS)
			newline(priv);
		draw_char(priv->col++, priv->row, c);
	}

	return 0;
}

static int gts9_fb_getc(struct udevice *dev)
{
	return -EAGAIN;
}

static int gts9_fb_pending(struct udevice *dev, bool input)
{
	return 0;
}

static int gts9_fb_setbrg(struct udevice *dev, int baudrate)
{
	return 0;
}

static int gts9_fb_probe(struct udevice *dev)
{
	struct gts9_fb_priv *priv = dev_get_priv(dev);
	const char *msg;

	/* Keep pre-relocation messages visible above later U-Boot output. */
	priv->row = (gd->flags & GD_FLG_RELOC) ? 15 : 0;
	msg = (gd->flags & GD_FLG_RELOC) ?
		"SM-X610 framebuffer serial: relocated\n" :
		"SM-X610 framebuffer serial: early\n";
	while (*msg)
		gts9_fb_putc(dev, *msg++);
	return 0;
}

static const struct dm_serial_ops gts9_fb_ops = {
	.putc = gts9_fb_putc,
	.getc = gts9_fb_getc,
	.pending = gts9_fb_pending,
	.setbrg = gts9_fb_setbrg,
};

static const struct udevice_id gts9_fb_ids[] = {
	{ .compatible = "samsung,gts9fepwifi-fb-serial" },
	{ }
};

U_BOOT_DRIVER(gts9_fb_serial) = {
	.name = "gts9_fb_serial",
	.id = UCLASS_SERIAL,
	.of_match = gts9_fb_ids,
	.probe = gts9_fb_probe,
	.ops = &gts9_fb_ops,
	.priv_auto = sizeof(struct gts9_fb_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
