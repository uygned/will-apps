#include "wl_bitmap.h"
#include <stdio.h>

inline int min(int a, int b) {
	return a < b ? a : b;
}

inline void rect_init(rect_s *rect, int left, int top, int width, int height) {
	rect->left = left;
	rect->top = top;
	rect->width = width;
	rect->height = height;
}

inline int rect_find(rect_s *rect, int x, int y) {
	return rect->top <= y && y < (rect->top + rect->height) && rect->left <= x
			&& x < (rect->left + rect->width);
}

void bitmap_copy(bitmap_t *bmp_dst, bitmap_t *bmp_src, int rotation) {
	if (bmp_dst == NULL || bmp_src == NULL)
		return;

	int src_w = bmp_src->rect.width;
	int src_h = bmp_src->rect.height;
	int src_pixel_size = bmp_src->bytes_per_pixel;
	int dst_pixel_size = bmp_dst->bytes_per_pixel;

	int dst_w = bmp_dst->rect.width;
	int dst_h = bmp_dst->rect.height;
	int src_line_size = bmp_src->bytes_per_line;
	int dst_line_size = bmp_dst->bytes_per_line;

//	printf(
//			"bitmap_copy: dst=%dx%d src=%dx%d pixel_size=%d/%d line_size=%d/%d\n",
//			dst_w, dst_h, src_w, src_h, dst_pixel_size, src_pixel_size,
//			dst_line_size, src_line_size);

	int x, y;
	unsigned char *src;
	unsigned char *dst;
	if (rotation == 0) {
		if (src_w != dst_w || src_h != dst_h)
			return;

		unsigned char *src_x = bmp_src->data;
		unsigned char *dst_y = bmp_dst->data;
		for (y = 0; y < src_h/*dst_w*/; y++) {
			src = src_x;
			dst = dst_y;
			for (x = 0; x < src_w/*dst_h*/; x++) {
				dst[0] = src[0];
				if (dst_pixel_size == 3) {
					dst[1] = src[1];
					dst[2] = src[2];
				}
				src += src_pixel_size;
				dst += dst_pixel_size;
			}
			src_x += src_line_size;
			dst_y += dst_line_size;
		}
	} else if (rotation == 270) {
		if (src_w != dst_h || src_h != dst_w)
			return;

		/*
		 * copy one src line to one dst column
		 * +--------+  +------+
		 * | >      |  |      |
		 * |        |=>|      |
		 * +--------+  | ^    |
		 *             +------+
		 */
		unsigned char *src_x = bmp_src->data;
		unsigned char *dst_y = bmp_dst->data + dst_line_size * (dst_h - 1);
		for (y = 0; y < src_h/*dst_w*/; y++) {
			src = src_x;
			dst = dst_y;
			for (x = 0; x < src_w/*dst_h*/; x++) {
				dst[0] = src[0];
				if (dst_pixel_size == 3) {
					dst[1] = src[1];
					dst[2] = src[2];
				}
				src += src_pixel_size;
				dst -= dst_line_size;
			}
			src_x += src_line_size;
			dst_y += dst_pixel_size;
		}
	}
}

void bitmap_dash_line(bitmap_t *canvas, int x, int y, int length) {
	unsigned char *dst = canvas->data + y * canvas->bytes_per_line
			+ x * canvas->bytes_per_pixel;
	int i, j;
	for (i = 0; i < length && i < canvas->rect.width; i++) {
		int c = i % 10 < 5 ? 0xff : 0;
		for (j = 0; j < canvas->bytes_per_pixel; j++)
			*dst++ = c;
	}
}

// target rect must lie in canvas rect
void bitmap_draw_rect(bitmap_t *canvas, rect_s *rect, unsigned char c) {
	if (rect->top + rect->height > canvas->rect.height
			|| rect->left + rect->width > canvas->rect.width)
		return;

	unsigned char *top = canvas->data + rect->top * canvas->bytes_per_line
			+ rect->left * canvas->bytes_per_pixel;
	unsigned char *bottom = top + (rect->height - 1) * canvas->bytes_per_line;
	unsigned char *left = top + canvas->bytes_per_line;
	unsigned char *right = left + (rect->width - 1) * canvas->bytes_per_pixel;

	int i, j;
	for (i = 0; i < rect->width; i++) {
		for (j = 0; j < canvas->bytes_per_pixel; j++) {
			*top++ = c;
			*bottom++ = c;
		}
	}
	for (i = 1; i < rect->height - 1; i++) {
		for (j = 0; j < canvas->bytes_per_pixel; j++) {
			left[j] = c;
			right[j] = c;
		}
		left += canvas->bytes_per_line;
		right += canvas->bytes_per_line;
	}
}

// target rect must lie in canvas rect
void bitmap_fill_rect(bitmap_t *canvas, rect_s *rect, unsigned char color_b,
		unsigned char color_g, unsigned char color_r) {
	if (rect->top + rect->height > canvas->rect.height
			|| rect->left + rect->width > canvas->rect.width)
		return;

//	printf("bitmap_fill_rect: T%d H%d L%d W%d\n", rect->top, rect->height,
//			rect->left, rect->width);

	char same_rgb = color_b == color_g && color_g == color_r;
	unsigned char *dst = canvas->data + rect->top * canvas->bytes_per_line
			+ rect->left * canvas->bytes_per_pixel;
	int i, j;
	for (j = 0; j < rect->height; j++) {
		if (same_rgb) {
			memset(dst, color_b, rect->width * canvas->bytes_per_pixel);
		} else {
			unsigned char *d = dst;
			for (i = 0; i < rect->width; i++) {
				*d++ = color_b;
				if (canvas->bytes_per_pixel == 3) {
					*d++ = color_g;
					*d++ = color_r;
				}
			}
		}
		dst += canvas->bytes_per_line;
	}
}

//void bitmap_fill_rect(bitmap_t *canvas, rect_s *rect, unsigned char c) {
//	unsigned char *dst = canvas->data + rect->top * canvas->bytes_per_line
//			+ rect->left * canvas->bytes_per_pixel;
//	int i, j, k;
//	for (j = 0; j < rect->height; j++) {
//		for (i = 0; i < rect->width; i++) {
//			unsigned char *d = dst;
//			for (k = 0; k < canvas->bytes_per_pixel; k++) {
//				*d++ = c;
//			}
//		}
//		dst += canvas->bytes_per_line;
//	}
//}
