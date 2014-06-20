#ifndef WL_FONT_H
#define WL_FONT_H

#include "wl_bitmap.h"
#include "ftimage.h"

void draw_line(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1,
		unsigned char color);
void draw_conic(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1/*control point*/,
		FT_Vector *p2, unsigned char color);

void displaced_downsample(unsigned char *dst, unsigned char *src, int dst_width,
		float filter_weights[]);

bitmap_t *raster_draw_contours(bitmap_t *canvas, FT_Outline *outline,
		int origin_x, int origin_y, float scale);
void raster_fill_contours(bitmap_t *dst, int dst_w, int dst_h, bitmap_t *src,
		int src_x, int src_y, int grid_width, int grid_height,
		unsigned char linear2srgb[]);
void raster_copy_contour(bitmap_t *dst, bitmap_t *src, int width, int height);

#endif
