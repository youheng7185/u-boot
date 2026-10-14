// SPDX-License-Identifier: GPL-2.0+
/* Console output to the RGB888 framebuffer left active by Samsung S-Boot. */
#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <video_font_8x16.h>
#include <asm/cache.h>
#include <linux/delay.h>
#include <linux/string.h>

#define FB_ADDR        0xa800035aUL
#define FB_WIDTH       540
#define FB_HEIGHT      960
#define FB_STRIDE      1620
#define FB_SIZE        (FB_STRIDE * FB_HEIGHT)
#define FONT_WIDTH     8
#define FONT_HEIGHT    16
#define TEXT_COLS      (FB_WIDTH / FONT_WIDTH)
#define TEXT_ROWS      (FB_HEIGHT / FONT_HEIGHT)

struct j2_fb_priv {
	unsigned int col;
	unsigned int row;
};

static void flush_fb_range(ulong start, ulong len)
{
	ulong line = ARCH_DMA_MINALIGN;
	ulong begin = start & ~(line - 1);
	ulong end = (start + len + line - 1) & ~(line - 1);

	flush_dcache_range(begin, end);
}

static void draw_char(unsigned int col, unsigned int row, unsigned char ch)
{
	unsigned char *fb = (unsigned char *)FB_ADDR;
	const unsigned char *glyph = video_fontdata_8x16 + ch * FONT_HEIGHT;
	unsigned int x, y;
	ulong first = FB_ADDR + row * FONT_HEIGHT * FB_STRIDE + col * FONT_WIDTH * 3;

	for (y = 0; y < FONT_HEIGHT; y++) {
		unsigned char *pixel = fb +
			(row * FONT_HEIGHT + y) * FB_STRIDE + col * FONT_WIDTH * 3;
		unsigned int bits = glyph[y];

		for (x = 0; x < FONT_WIDTH; x++) {
			unsigned char value = (bits & (0x80 >> x)) ? 0xff : 0x00;

			pixel[0] = value;
			pixel[1] = value;
			pixel[2] = value;
			pixel += 3;
		}
	}
	flush_fb_range(first, (FONT_HEIGHT - 1) * FB_STRIDE + FONT_WIDTH * 3);
}

static void scroll_up(void)
{
	unsigned char *fb = (unsigned char *)FB_ADDR;
	unsigned int bytes = (TEXT_ROWS - 1) * FONT_HEIGHT * FB_STRIDE;
	unsigned int i;

	/* Use a forward byte copy because source and destination overlap. */
	for (i = 0; i < bytes; i++)
		fb[i] = fb[i + FONT_HEIGHT * FB_STRIDE];
	for (; i < FB_SIZE; i++)
		fb[i] = 0;
	flush_fb_range(FB_ADDR, FB_SIZE);
}

static void newline(struct j2_fb_priv *priv)
{
	priv->col = 0;
	if (++priv->row >= TEXT_ROWS) {
		scroll_up();
		priv->row = TEXT_ROWS - 1;
	}
}

static int j2_fb_putc(struct udevice *dev, const char ch)
{
	struct j2_fb_priv *priv = dev_get_priv(dev);
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

static int j2_fb_getc(struct udevice *dev)
{
	return -EAGAIN;
}

static int j2_fb_pending(struct udevice *dev, bool input)
{
	return 0;
}

static int j2_fb_setbrg(struct udevice *dev, int baudrate)
{
	return 0;
}

static const struct dm_serial_ops j2_fb_ops = {
	.putc = j2_fb_putc,
	.getc = j2_fb_getc,
	.pending = j2_fb_pending,
	.setbrg = j2_fb_setbrg,
};

static const struct udevice_id j2_fb_ids[] = {
	{ .compatible = "samsung,j2y18lte-fb-console" },
	{ }
};

U_BOOT_DRIVER(j2_fb_serial) = {
	.name = "j2_fb_serial",
	.id = UCLASS_SERIAL,
	.of_match = j2_fb_ids,
	.ops = &j2_fb_ops,
	.priv_auto = sizeof(struct j2_fb_priv),
	.flags = DM_FLAG_PRE_RELOC,
};
