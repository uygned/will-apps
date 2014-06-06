#ifndef WL_BITMAP_H_
#define WL_BITMAP_H_

#include <string.h>
#include <stdint.h>

//#ifdef KINDLE
//typedef struct {
//	unsigned char r;
//}color_t;
//#else
typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} color_t;
//#endif

typedef struct {
	int left;
	int top;
	int width;
	int height;
} rect_s;

typedef struct {
	rect_s rect;
	unsigned char *data;
	unsigned char bytes_per_pixel;
	uint32_t bytes_per_line;
	uint32_t depth;
} bitmap_t;

int min(int a, int b);

void rect_init(rect_s *rect, int left, int top, int width, int height);
int rect_find(rect_s *rect, int x, int y);

void bitmap_copy(bitmap_t *dst, bitmap_t *src, int rotation);

void bitmap_dash_line(bitmap_t *canvas, int x, int y, int length);

// target rect must lie in canvas rect
void bitmap_draw_rect(bitmap_t *canvas, rect_s *rect, unsigned char c);

// target rect must lie in canvas rect
void bitmap_fill_rect(bitmap_t *canvas, rect_s *rect, unsigned char color_b,
		unsigned char color_g, unsigned char color_r);

//static void bitmap_fill_rect(bitmap_t *canvas, rect_s *rect, unsigned char c) {
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

#endif
