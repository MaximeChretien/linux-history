/*
 * linux/drivers/video/fbgen.c -- Generic routines for frame buffer devices
 *
 *  Created 3 Jan 1998 by Geert Uytterhoeven
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/fb.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/io.h>

int cfb_cursor(struct fb_info *info, struct fbcursor *cursor)
{
	int i, size = ((cursor->size.x + 7) / 8) * cursor->size.y;
	struct fb_image image;
	static char data[64];

	image.bg_color = cursor->index->entry[0];
	image.fg_color = cursor->index->entry[1];

	if (cursor->depth == 1) {
		if (cursor->enable) {
			switch (cursor->rop) {
			case ROP_XOR:
				for (i = 0; i < size; i++)
					data[i] = (cursor->image[i] &
						   cursor->mask[i]) ^
					    	   cursor->dest[i];
				break;
			case ROP_COPY:
			default:
				for (i = 0; i < size; i++)
					data[i] =
					    cursor->image[i] & cursor->mask[i];
				break;
			}
		} else
			memcpy(data, cursor->dest, size);

		image.dx = cursor->pos.x;
		image.dy = cursor->pos.y;
		image.width = cursor->size.x;
		image.height = cursor->size.y;
		image.depth = cursor->depth;
		image.data = data;

		if (info->fbops->fb_imageblit)
			info->fbops->fb_imageblit(info, &image);
	}
	return 0;
}

int fb_blank(int blank, struct fb_info *info)
{
	struct fb_cmap cmap;
	u16 black[16];

	if (info->fbops->fb_blank && !info->fbops->fb_blank(blank, info))
		return 0;
	if (blank) {
		memset(black, 0, 16 * sizeof(u16));
		cmap.red = black;
		cmap.green = black;
		cmap.blue = black;
		cmap.transp = NULL;
		cmap.start = 0;
		cmap.len = 16;
		fb_set_cmap(&cmap, 1, info);
	} else {
		if (info->cmap.len)
			fb_set_cmap(&info->cmap, 1, info);
		else {
			int size =
			    info->var.bits_per_pixel == 16 ? 64 : 256;
			fb_set_cmap(fb_default_cmap(size), 1, info);
		}
	}
	return 0;
}

/* generic frame buffer operations */
EXPORT_SYMBOL(cfb_cursor);
EXPORT_SYMBOL(fb_blank);

MODULE_LICENSE("GPL");
