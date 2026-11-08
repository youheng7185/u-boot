/* SPDX-License-Identifier: GPL-2.0+ */
/* Temporary SM-X610 relocation checkpoints on the inherited framebuffer. */
#ifndef __ASM_GTS9_PROGRESS_H
#define __ASM_GTS9_PROGRESS_H

#ifdef CONFIG_GTS9FEPWIFI
	/* Row y=208, 64x32 pixels. Clobbers only x13-x17. */
	.macro gts9_reloc_mark xoff, low, high
	movz	x14, #0x5000
	movk	x14, #0xfa34, lsl #16
	add	x14, x14, #\xoff
	movz	w15, #\low
	movk	w15, #\high, lsl #16
	mov	w17, #32
	mov	x13, #6144
.Lgts9_row\@:
	mov	w16, #64
.Lgts9_pixel\@:
	str	w15, [x14], #4
	subs	w16, w16, #1
	b.ne	.Lgts9_pixel\@
	add	x14, x14, x13
	subs	w17, w17, #1
	b.ne	.Lgts9_row\@
	dsb	sy
	.endm
#endif

#endif
