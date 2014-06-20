#ifndef WL_FONT_H
#define WL_FONT_H

#include "wl_bitmap.h"
#include "ftimage.h" /* FT_Outline */

#define FIR5_FILTER 0
#define CUSTOME_FILTER 1
#define DISPLACED_FILTER 2
/* displaced filter with oversampling by RGB luminance weights */
#define DISPLACED_WEIGHTED 3

void displaced_downsample(unsigned char *dst, unsigned char *src, int dst_width,
		float filter_weights[]);

void font_draw_line(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1,
		unsigned char color);

bitmap_t *font_draw_contours(bitmap_t *canvas, FT_Outline *outline,
		int origin_x, int origin_y, float scale);

void font_fill_contours(bitmap_t *dst, int dst_w, int dst_h, bitmap_t *src,
		int src_x, int src_y, int grid_width, int grid_height,
		unsigned char linear2srgb[]);

void font_paint_raw(bitmap_t *dst, bitmap_t *src, int width, int height);

#endif
