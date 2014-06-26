#include "wl_font.h"
#include "wl_math.h"
#include "bresenham_rasterizer.h"
#include <stdio.h>
#include <stdlib.h>

//#define FONT_DEBUG
//#define LABEL_CONTOUR_POINTS

static int subpixel_rendering = 1;
static int subpixel_filter_type = DISPLACED_WEIGHTED;

//float fir5_filter[] = { 0.119, 0.370, 0.511, 0.370, 0.119 }; // mitchell_filter
//float fir5_filter[] = { 0.090, 0.373, 0.536, 0.373, 0.090 }; // catmull_rom_filter
//float fir5_filter[] = { -0.106, 0.244, 0.726, 0.244, -0.106 };
//float fir5_filter[] = { 0.059, 0.235, 0.412, 0.235, 0.059 };
//float fir5_filter[] = { 0., 1. / 4., 2. / 4., 1. / 4., 0. };
static float fir5_filter[] = { 1. / 17., 4. / 17., 7. / 17., 4. / 17., 1. / 17. };

//static float displaced_filter[] = { -0.0955, -0.3090, 0.25, 0.25, 0.809,
//		0.25, 0.25, -0.3090, -0.0955 };
//static float displaced_filter[] = { -0.1190, 0.1003, 0.3197, 0.5392,
//				0.3197, 0.1003 , -0.1190 };
//static float displaced_filter[] = { -0.0829, 0.0586, 0.3414, 0.4830,
//		0.3414, 0.0586, -0.0829 };
//static float displaced_filter[] = { -0.0586, -0.0829, 0.0586, 0.3414,
//		0.4830, 0.3414, 0.0586, -0.0829, -0.0586 };
//static float displaced_filter[] = { 0, 0, 0.3333, 0.3334, 0.3333, 0, 0 };
static float displaced_filter[] = { 0.3333, 0.3334, 0.3333 };
//static float displaced_filter[] = { 0.3, 0.4, 0.3 };
//static float displaced_filter[] = { 0.45, 0.35, 0.2 };

static int displaced_filter_length = sizeof(displaced_filter) / sizeof(float);

/* Downsamples the oversampled 3 pixels to one pixel (9 subpixels to 3 subpixels).
 * [RGB](i) <- [RGB](3i) [RGB](3i+1) [RGB](3i+2)
 * Papers:
 * A filter design algorithm for subpixel rendering on matrix displays (2007)
 * Optimizing subpixel rendering using a perceptual metric (2011)
 */
void displaced_downsample(unsigned char *dst, unsigned char *src, int dst_width,
		float filter_weights[]) {
	int dst_pixel_size = 3;
	int src_triple_size = 9;

	unsigned char *d = dst;
	unsigned char *s = src, r_prev = 0;
	int i = 0;
	while (i++ < dst_width) {
//		d[2] = ((unsigned) r_prev + s[0] + s[3]) * 1. / 3.;
//		d[1] = ((unsigned) s[1] + s[4] + s[7]) * 1. / 3.;
//		d[0] = ((unsigned) s[5] + s[8] + s[11]) * 1. / 3.;
		/* Rgb-Rgb_Rgb_rgb-rgb */
		d[0] = float2byte(filter_weights[0] * r_prev + //
				filter_weights[1] * s[0] +  //
				filter_weights[2] * s[3]);
		/* rgb-rGb_rGb_rGb-rgb */
		d[1] = float2byte(filter_weights[0] * s[1] +   //
				filter_weights[1] * s[4] +  //
				filter_weights[2] * s[7]);
		/* rgb-rgb_rgB_rgB-rgB */
		d[2] = float2byte(filter_weights[0] * s[5] +   //
				filter_weights[1] * s[8] +  //
				filter_weights[2] * (i < dst_width ? s[11] : 0));

//		d[0] = ((unsigned) s[2] + s[5] + s[8]) / 3.;
//		d[1] = ((unsigned) s[1] + s[4] + s[7]) / 3.;
//		d[2] = ((unsigned) s[0] + s[3] + s[6]) / 3.;

		r_prev = s[6];
		d += dst_pixel_size;
		s += src_triple_size;
	}
}

void displaced_downsample5(unsigned char *dst, unsigned char *src,
		int dst_width, float filter_weights[]) {
	int dst_pixel_size = 3;
	int src_triple_size = 9;

	unsigned char *d = dst;
	unsigned char *s = src, r_prpr = 0, r_prev = 0, g_prev = 0;
	int i = 0;
	while (i++ < dst_width) {
		/* Rgb_Rgb-Rgb_Rgb_Rgb-rgb_rgb */
		d[0] = float2byte(filter_weights[0] * r_prpr + //
				filter_weights[1] * r_prev + //
				filter_weights[2] * s[0] + //
				filter_weights[3] * s[3] + //
				filter_weights[4] * s[6]);
		/* rgb_rGb-rGb_rGb_rGb-rGb_rgb */
		d[1] = float2byte(filter_weights[0] * g_prev + //
				filter_weights[1] * s[1] + //
				filter_weights[2] * s[4] + //
				filter_weights[3] * s[7] + //
				filter_weights[4] * (i < dst_width ? s[10] : 0));
		/* rgb_rgb-rgB_rgB_rgB-rgB_rgB */
		d[2] = float2byte(filter_weights[0] * s[2] + //
				filter_weights[1] * s[5] + //
				filter_weights[2] * s[8] + //
				filter_weights[3] * (i < dst_width ? s[11] : 0) + //
				filter_weights[4] * (i < dst_width ? s[14] : 0));

//		d[0] = ((unsigned) s[2] + s[5] + s[8]) / 3.;
//		d[1] = ((unsigned) s[1] + s[4] + s[7]) / 3.;
//		d[2] = ((unsigned) s[0] + s[3] + s[6]) / 3.;

		r_prpr = s[3];
		r_prev = s[6];
		g_prev = s[7];
		d += dst_pixel_size;
		s += src_triple_size;
	}
}

void displaced_downsample7(unsigned char *dst, unsigned char *src,
		int dst_width, float filter_weights[]) {
	int dst_pixel_size = 3;
	int src_triple_size = 9;

	unsigned char *d = dst;
	unsigned char *s = src, r_prpr = 0, r_pppr = 0, r_prev = 0, g_prpr = 0,
			g_prev = 0, b_prev = 0;
	int i = 0;
	while (i++ < dst_width) {
		/* Rgb_Rgb-Rgb_Rgb_Rgb-rgb_rgb */
		d[0] = float2byte(filter_weights[0] * r_pppr + //
				filter_weights[1] * r_prpr + //
				filter_weights[2] * r_prev + //
				filter_weights[3] * s[0] + //
				filter_weights[4] * s[3] + //
				filter_weights[5] * s[6] + //
				filter_weights[6] * (i < dst_width ? s[9] : 0));
		/* rgb_rGb-rGb_rGb_rGb-rGb_rgb */
		d[1] = float2byte(filter_weights[0] * g_prpr + //
				filter_weights[1] * g_prev + //
				filter_weights[2] * s[1] + //
				filter_weights[3] * s[4] + //
				filter_weights[4] * s[7] + //
				filter_weights[5] * (i < dst_width ? s[10] : 0) + //
				filter_weights[6] * (i < dst_width ? s[13] : 0));
		/* rgb_rgb-rgB_rgB_rgB-rgB_rgB */
		d[2] = float2byte(filter_weights[0] * b_prev + //
				filter_weights[1] * s[2] + //
				filter_weights[2] * s[5] + //
				filter_weights[3] * s[8] + //
				filter_weights[4] * (i < dst_width ? s[11] : 0) + //
				filter_weights[5] * (i < dst_width ? s[14] : 0) + //
				filter_weights[6] * (i < dst_width ? s[17] : 0));

//		d[0] = ((unsigned) s[2] + s[5] + s[8]) / 3.;
//		d[1] = ((unsigned) s[1] + s[4] + s[7]) / 3.;
//		d[2] = ((unsigned) s[0] + s[3] + s[6]) / 3.;

		r_pppr = s[0];
		r_prpr = s[3];
		r_prev = s[6];
		g_prpr = s[4];
		g_prev = s[7];
		b_prev = s[8];
		d += dst_pixel_size;
		s += src_triple_size;
	}
}

void displaced_downsample9(unsigned char *dst, unsigned char *src,
		int dst_width, float filter_weights[]) {
	int dst_pixel_size = 3;
	int src_triple_size = 9;

	unsigned char *d = dst;
	unsigned char *s = src, r_pppp = 0, r_pppr = 0, r_prpr = 0, r_prev = 0,
			g_pppr = 0, g_prpr = 0, g_prev = 0, b_prpr = 0, b_prev = 0;
	int i = 0;
	while (i++ < dst_width) {
		/* Rgb_Rgb-Rgb_Rgb_Rgb-rgb_rgb */
		d[0] = float2byte(filter_weights[0] * r_pppp + //
				filter_weights[1] * r_pppr + //
				filter_weights[2] * r_prpr + //
				filter_weights[3] * r_prev + //
				filter_weights[4] * s[0] + //
				filter_weights[5] * s[3] + //
				filter_weights[6] * s[6] + //
				filter_weights[7] * (i < dst_width ? s[9] : 0) + //
				filter_weights[8] * (i < dst_width ? s[12] : 0));
		/* rgb_rGb-rGb_rGb_rGb-rGb_rgb */
		d[1] = float2byte(filter_weights[0] * g_pppr + //
				filter_weights[1] * g_prpr + //
				filter_weights[2] * g_prev + //
				filter_weights[3] * s[1] + //
				filter_weights[4] * s[4] + //
				filter_weights[5] * s[7] + //
				filter_weights[6] * (i < dst_width ? s[10] : 0) + //
				filter_weights[7] * (i < dst_width ? s[13] : 0) + //
				filter_weights[8] * (i < dst_width ? s[16] : 0));
		/* rgb_rgb-rgB_rgB_rgB-rgB_rgB */
		d[2] = float2byte(filter_weights[0] * b_prpr + //
				filter_weights[1] * b_prev + //
				filter_weights[2] * s[2] + //
				filter_weights[3] * s[5] + //
				filter_weights[4] * s[8] + //
				filter_weights[5] * (i < dst_width ? s[11] : 0) + //
				filter_weights[6] * (i < dst_width ? s[14] : 0) + //
				filter_weights[7] * (i < dst_width ? s[17] : 0) + //
				filter_weights[8] * ((i + 1) < dst_width ? s[20] : 0));

//		d[0] = ((unsigned) s[2] + s[5] + s[8]) / 3.;
//		d[1] = ((unsigned) s[1] + s[4] + s[7]) / 3.;
//		d[2] = ((unsigned) s[0] + s[3] + s[6]) / 3.;

		if (i > 0)
			r_pppr = s[-3];
		r_pppr = s[0], r_prpr = s[3], r_prev = s[6];
		g_prpr = s[1], g_prpr = s[4], g_prev = s[7];
		/*           */b_prpr = s[5], b_prev = s[8];
		d += dst_pixel_size;
		s += src_triple_size;
	}
}

static unsigned char contour_colors[3] = { 1, 2 };
static unsigned char slimming_color = 4;
static unsigned char filling_color = 0xff;
static unsigned char debug_color = 0x80;

static inline void font_set_contour(unsigned char *d, unsigned char color) {
	if (d[0] != slimming_color)
		d[0] = color;
}

/* for bresenham_rasterizer */
static unsigned char contour_color = 0xff;

inline void font_set_contourl(unsigned char *d, int x, int y,
		int bytes_per_pixel, int bytes_per_line) {
	if (x < 0 || y < 0) {
		printf("error contour point: %d %d\n", x, y);
		return;
	}
	font_set_contour(d + y * bytes_per_line + x * bytes_per_pixel,
			contour_color);
}

void font_draw_line(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1,
		unsigned char color) {
	contour_color = color;
//	plotLine(p0->x, p0->y, p1->x, p1->y);
//	return;
//	unsigned char *data = canvas->data
//			+ canvas->rect.top * canvas->bytes_per_line
//			+ canvas->rect.left * canvas->bytes_per_pixel;
	unsigned char *data = canvas->data;
	int x, y;
	if (p0->x == p1->x) {
		if (p0->y == p1->y) {
			// draw one pixel
			data += p0->y * canvas->bytes_per_line
					+ p0->x * canvas->bytes_per_pixel;
			font_set_contour(data, color);
		} else {
			// draw vertical line
			if (p0->y > p1->y) {
				FT_Vector *p = p0;
				p0 = p1;
				p1 = p;
			}

			data += p0->y * canvas->bytes_per_line
					+ p0->x * canvas->bytes_per_pixel;
			for (y = p0->y; y <= p1->y; y++) {
				font_set_contour(data, color);
				data += canvas->bytes_per_line;
			}
		}
	} else {
		if (p0->x > p1->x) {
			FT_Vector *p = p0;
			p0 = p1;
			p1 = p;
		}

		if (p0->y == p1->y) {
			// draw horizontal line
			data += p0->y * canvas->bytes_per_line
					+ p0->x * canvas->bytes_per_pixel;
			for (x = p0->x; x <= p1->x; x++) {
				font_set_contour(data, color);
				data += canvas->bytes_per_pixel;
			}
		} else {
			plotLine(p0->x, p0->y, p1->x, p1->y, canvas->data,
					canvas->bytes_per_line, canvas->bytes_per_pixel);
		}
	}
}

static void font_draw_conic(bitmap_t *canvas, FT_Vector *p0,
		FT_Vector *p1/*conic*/, FT_Vector *p2, unsigned char color) {
	contour_color = color;
//	printf("font_draw_conic: %ld.%ld %ld.%ld %ld.%ld", p0->x, p0->y, p1->x, p1->y,
//			p2->x, p2->y);
	plotQuadBezier(p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, canvas->data,
			canvas->bytes_per_line, canvas->bytes_per_pixel);
}

static void font_draw_cubic(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1,
		FT_Vector *p2, FT_Vector *p3, unsigned char color) {
	contour_color = color;
	plotCubicBezier(p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, p3->x, p3->y,
			canvas->data, canvas->bytes_per_line, canvas->bytes_per_pixel);
}

static inline int is_same_point(FT_Vector *p0, FT_Vector *p1) {
	return p0->x == p1->x && p0->y == p1->y;
}

static inline int font_contour_on(unsigned char *s) {
	return s[0] == contour_colors[0] || s[0] == contour_colors[1];
}

#define PREV_ON 1
#define PREV_ON_ABOVE 2
#define PREV_ON_BELOW 4
#define BEFOR_ON_FILL 8

inline int font_contour_around(unsigned char *s, int bytes_per_line) {
	int bytes_per_pixel = 1;
	unsigned char *a = s - bytes_per_line;
	unsigned char *b = s + bytes_per_line;
	int above_on = font_contour_on(a - bytes_per_pixel) || font_contour_on(a)
			|| font_contour_on(a + bytes_per_pixel);
	int below_on = font_contour_on(b - bytes_per_pixel) || font_contour_on(b)
			|| font_contour_on(b + bytes_per_pixel);
	if (above_on && below_on)
		return 0; // return PREV_ON_ABOVE | PREV_ON_BELOW;
	if (above_on)
		return PREV_ON_ABOVE;
	if (below_on)
		return PREV_ON_BELOW;
	return 0;
}

static int font_slim_contour(bitmap_t *canvas, FT_Vector *p0) {
	if (p0->x < 0 || p0->x >= canvas->rect.width || p0->y < 0
			|| p0->y >= canvas->rect.height)
		return 0;

	unsigned char *s = canvas->data + p0->y * canvas->bytes_per_line
			+ p0->x * canvas->bytes_per_pixel;
	unsigned char *t = s - canvas->bytes_per_line;

	// line above
	int a = font_contour_on(t);
	int al = font_contour_on(t - canvas->bytes_per_pixel);
	int ar = font_contour_on(t + canvas->bytes_per_pixel);

	// line below
	t = s + canvas->bytes_per_line;
	int b = font_contour_on(t);
	int bl = font_contour_on(t - canvas->bytes_per_pixel);
	int br = font_contour_on(t + canvas->bytes_per_pixel);

	// left and right
	int l = font_contour_on(s - canvas->bytes_per_pixel);
	int r = font_contour_on(s + canvas->bytes_per_pixel);

	char slim = 0;
	if (!(a || al || ar)) { // no above, growing down
		if (b) {
			/* above: ---    ---
			 *        *x* => *-*
			 * below: *x*    *x*  (connectivity is kept via b)
			 */
			slim = 1;

			/* size-4 block:
			 * above: ---    ---
			 *        *x* => ---  (l and r must be off)
			 * below: xx*    *x*
			 *        xx*    *x*
			 */
			if (bl || br) {
				s[0] = slimming_color;
				FT_Vector p = *p0;
				p.y++;
				font_slim_contour(canvas, &p);
				if (bl) {
					p.x--;
					font_slim_contour(canvas, &p);
				} else if (br) {
					p.x++;
					font_slim_contour(canvas, &p);
				}
			}
			/* above: ---    ---
			 *        -x- => ---
			 * below: -x-    -x-
			 */
			else if (!(l || r)) {
				s[0] = slimming_color;
				FT_Vector p = *p0;
				p.y++;
				font_slim_contour(canvas, &p);
			}
		} else if (!(l || bl) || !(r || br)) {
			/* no left or no right
			 * above: ---    ---
			 *        -x* => --*
			 * below: --*    --*  (connectivity is kept via b)
			 */
			slim = 1;
		}
	} else if (!(b || bl || br)) { // no below, growing up
		if (a) {
			slim = 1;
			if (al || ar) {
				s[0] = slimming_color;
				FT_Vector p = *p0;
				p.y--;
				font_slim_contour(canvas, &p);
				if (al) {
					p.x--;
					font_slim_contour(canvas, &p);
				} else if (ar) {
					p.x++;
					font_slim_contour(canvas, &p);
				}
			} else if (!(l || r)) {
				s[0] = slimming_color;
				FT_Vector p = *p0;
				p.y--;
				font_slim_contour(canvas, &p);
			}
		} else if (!(l || al) || !(r || ar)) {
			slim = 1;
		}
	}

#ifdef z
	int n = l + r + a + b + al + ar + bl + br;
	switch (n) {
		case 1:
		slim = a || b; /* just check vertical because scan line is horizontal */
		break;
		case 2:
		slim = (a && (al || ar)) || (b && (bl || br)); /* angle */
		if (!slim)
		slim = (a && (l || r)) || (b && (l || r));
		break;
		case 3:
		slim = a && ((l && al) || (r && ar));
		if (!slim)
		slim = b && ((l && bl) || (r && br));
		break;
	}
#endif

	if (slim) {
		s[0] = slimming_color;
		return 1;
	}
	return 0;

	/* four cases for start point:
	 *   *  *  *+  +*
	 *  *+  +*  *  *
	 */
//	if ((a || b) && (l || r))
//		s[clean_c] = 0;
//	/* or only one on around */
}

void font_fill_contours(bitmap_t *dst, int dst_w, int dst_h, bitmap_t *src,
		int src_x, int src_y, int grid_width, int grid_height,
		unsigned char linear2srgb[]) {
	int src_line_size = src->bytes_per_line;
	int src_pixel_size = src->bytes_per_pixel;
	unsigned char *src_line_ = src->data + src_y * src_line_size
			+ src_x * src_pixel_size;

	int dst_line_size = dst->bytes_per_line;
	int dst_pixel_size = dst->bytes_per_pixel;
	unsigned char *dst_line = dst->data;

	int src_line_size_ = src_line_size * grid_height;
	int src_pixel_size_ = src_pixel_size * grid_width;

	int i, j, k;
	unsigned char *src_line, *s_, *s, *d;

	unsigned grid_area = grid_width * grid_height;
	float weight;

	char rgb_weighted = subpixel_filter_type == DISPLACED_WEIGHTED;

	/* subpixel rendering via grid partitioning, one grid partition corresponds one subpixel */
	int part_count = 0;
	unsigned *part_ends = NULL;
	unsigned *part_areas = NULL;
	float *part_weights = NULL;
	unsigned char *subpixel_line = NULL;
	unsigned subpixel_line_size;
	if (subpixel_rendering) {
		if (subpixel_filter_type == DISPLACED_FILTER || rgb_weighted) {
			/* 3 pixels per grid are required to do displaced downsampling */
			part_count = 9;
			/* partition areas for one pixel should follow P(R):P(G):P(B)=3:6:1 */
			/* but sometimes 3:5:2 is used */
			if (rgb_weighted && grid_width < (3 + 6 + 1) * 3)
				perror("error DISPLACED_WEIGHTED");
			subpixel_line_size = dst_w * part_count;
		} else {
			/* subpixel rendering one pixel per grid */
			part_count = 3;
			/* add 2 subpixels on both left side and right side for FIR5 filtering */
			subpixel_line_size = dst_w * part_count + 4;
		}

		part_ends = malloc(sizeof(unsigned) * part_count);
		part_areas = malloc(sizeof(unsigned) * part_count);
		part_weights = malloc(sizeof(float) * part_count);
		subpixel_line = malloc(subpixel_line_size);

#ifdef FONT_DEBUG
		printf("partition widths:");
#endif
		j = 0;
		for (i = 0; i < part_count; i++) {
			if (rgb_weighted)
				k = i % 3 == 0 ? 3 : (i % 3 == 1 ? 6 : 1);
			else
				k = (grid_width - j) / (part_count - i);
			j += k;
			part_areas[i] = k * grid_height;
			part_ends[i] = j;
#ifdef FONT_DEBUG
			printf(" %02d", k);
#endif
		}
#ifdef FONT_DEBUG
		printf("\npartition ends:  ");
		for (i = 0; i < part_count; i++)
		printf(" %02d", part_ends[i]);
		printf("\n");
#endif
	}

	char *prev = malloc(grid_height);
	char *fill = malloc(grid_height);
	int gj, gi;
	for (gj = 0; gj < dst_h; gj++, src_line_ += src_line_size_, dst_line +=
			dst_line_size) {
		d = dst_line;
		s_ = src_line_;

		memset(prev, 0, grid_height);
		memset(fill, 0, grid_height);
		if (subpixel_line)
			memset(subpixel_line, 0, subpixel_line_size);

		for (gi = 0; gi < dst_w; gi++, s_ += src_pixel_size_) {
			src_line = s_;

			if (part_weights)
				memset(part_weights, 0, sizeof(float) * part_count);
			else
				weight = 0;

			for (j = 0; j < grid_height; j++, src_line += src_line_size) {
				s = src_line;
				k = 0; /* partition index */
				for (i = 0; i < grid_width; i++, s += src_pixel_size) {
					if (i >= part_ends[k])
						k++;

//					if (gi == 5 && gj == 7 && i == 2 && j == 5)
//						s[0] = debug_color;

					/* convex (up and down) curves: \_/ and /`\
					 *     *--a (change on a)/(recover to BEFOR_ON_FILL)
					 *    /    \
					 *   /  *b  \ (change on b?)/(recover to BEFOR_ON_FILL)
					 *  /  /  \  \
					 * *--*    *--*
					 */
					if (font_contour_on(s)) {
						if (!prev[j]) {
							//  (off) on
							prev[j] = PREV_ON | (fill[j] ? BEFOR_ON_FILL : 0);
							int a = font_contour_around(s, src_line_size);
							prev[j] |= a;
							fill[j] = !fill[j];
						} //EL  (on)  on
					} else {
						if (prev[j]) {
							//  (on) off
							int a = font_contour_around(s - src_pixel_size,
									src_line_size);
							if (a && (prev[j] & a))
								fill[j] = prev[j] & BEFOR_ON_FILL;
							prev[j] = 0;
						} //EL (off) off
					}

					if (fill[j]) {
						if (s[0] == 0)
							s[0] = filling_color;
					}

					if (s[0] > 0) {
						if (part_weights)
							part_weights[k]++;
						else
							weight++;
					}
				}
			} /* j */

			/* grid/part weight ready */

			if (subpixel_rendering) {
				for (i = 0; i < part_count; i++) {
					if (part_weights[i] == 0)
						continue;

					weight = part_weights[i] / part_areas[i];
					if (weight < 0 || weight > 1)
						perror("weight > 1");
//					printf("%f %.0f/%d\n", weight, part_weights[k], part_areas[k]);

					j = gi * part_count + i; /* subpixel index */
					if (subpixel_filter_type == DISPLACED_FILTER
							|| rgb_weighted) {
						subpixel_line[j] = float2byte(weight * 255.0);
					} else if (subpixel_filter_type == CUSTOME_FILTER) {
						subpixel_line[j + 2] = float2byte(weight * 255.0);
					} else {
						/* FIR5 filtering */
						for (k = 0; k < 5; k++) {
							float f = subpixel_line[j + k]
									+ fir5_filter[k] * weight * 255.0;
							subpixel_line[j + k] = float2byte(f);
						}
					}
				}
			} else if (weight > 0) {
				weight = weight / grid_area;
				d[0] = float2byte(weight * 255.0);
				d[1] = d[0];
				d[2] = d[0];
//				printf("%d %d = %.0f/%.0f\n", x, y, weight, grid_area);
				d += dst_pixel_size;
			}
		} /* gi */

		if (subpixel_rendering) {
			if (subpixel_filter_type == DISPLACED_FILTER || rgb_weighted) {
				if (displaced_filter_length == 3) {
					displaced_downsample(d, subpixel_line, dst_w,
							displaced_filter);
				} else if (displaced_filter_length == 5) {
					displaced_downsample5(d, subpixel_line, dst_w,
							displaced_filter);
				} else if (displaced_filter_length == 7) {
					displaced_downsample7(d, subpixel_line, dst_w,
							displaced_filter);
				} else if (displaced_filter_length == 9) {
					displaced_downsample9(d, subpixel_line, dst_w,
							displaced_filter);
				}
				/* dump bytes */
//				for (i = 0; i < subpixel_line_size; i += 3) {
//					printf(" %02X%02X%02X", subpixel_line[i], subpixel_line[i + 1],
//							subpixel_line[i + 2]);
//				}
//				printf("\n");
//				for (i = 0; i < dst_w; i++) {
//					j = i * dst_pixel_size;
//					printf(" %02X%02X%02X", 0xff - d[j], 0xff - d[j + 1],
//							0xff - d[j + 2]);
//				}
//				printf("\n");
				if (linear2srgb) {
					i = 0;
					while (i++ < dst_w) {
						k = d[0];
						d[0] = linear2srgb[0xff - d[2]];
						d[1] = linear2srgb[0xff - d[1]];
						d[2] = linear2srgb[0xff - k];
						d += dst_pixel_size;
					}
				}
			} else {
				if (subpixel_filter_type == CUSTOME_FILTER) {
					unsigned char *p = subpixel_line + 2;
					for (i = 2; i < subpixel_line_size - 2; i++) {
						if (p[i] < 0x80)
							continue;
						if (p[i - 2] == 0 && p[i - 1] < 0x80) {
							p[i - 2] = p[i] / 16. * 1;
							p[i - 1] = p[i] / 16. * 5;
//							p[i] = p[i] - p[i - 1] - p[i - 2];
							p[i] = p[i] / 16.0 * 10;
						}
						if (p[i + 1] < 0x80 && p[i + 2] == 0) {
							p[i + 1] = p[i] / 16. * 5;
							p[i + 2] = p[i] / 16. * 1;
//							p[i] = p[i] - p[i + 1] - p[i + 2];
							p[i] = p[i] / 16.0 * 10;
						}
					}
				}
				for (i = 2; i < subpixel_line_size - 2;) {
					if (linear2srgb) {
						d[2] = linear2srgb[0xff - subpixel_line[i++]];
						d[1] = linear2srgb[0xff - subpixel_line[i++]];
						d[0] = linear2srgb[0xff - subpixel_line[i++]];
					} else {
						d[0] = subpixel_line[i++];
						d[1] = subpixel_line[i++];
						d[2] = subpixel_line[i++];
					}
					d += dst_pixel_size;
				}
			}
		}
	}

	free(prev);
	free(fill);
	if (part_ends)
		free(part_ends);
	if (part_weights)
		free(part_weights);
	if (subpixel_line)
		free(subpixel_line);
}

static inline int extra_right(int x, int grid_size, int margin) {
	if (x > 0) {
		/* 0 .. 10 .. 20 .. 30 */
		/* [20, 29] */
		if ((x % grid_size) >= (grid_size * 2. / 3.))
			return grid_size;
	} else {
		/* -30 .. -20 .. -10 .. 0 */
		/* [-10, -1] + 30 = [20, 29] */
		if ((x % grid_size) + grid_size >= (grid_size * 2. / 3.))
			return grid_size;
	}
	return margin;
}

static inline int extra_left(int x, int grid_size, int margin) {
	if (x > 0) {
		/* 0 .. 10 .. 20 .. 30 */
		/* [0, 9] */
		if ((x % grid_size) < (grid_size * 1. / 3.))
			return grid_size;
	} else {
		/* -30 .. -20 .. -10 .. 0 */
		/* [-30, -21] + grid_size = [0, 9] */
		if ((x % grid_size) + grid_size < (grid_size * 1. / 3.))
			return grid_size;
	}
	return margin;
}

/* http://en.wikipedia.org/wiki/Rasterisation#Scan_conversion */
/* http://en.wikipedia.org/wiki/Scanline_algorithm */
bitmap_t *font_draw_contours(bitmap_t *canvas, FT_Outline *outline,
		int origin_x, int origin_y, int grid_size) {
	if (outline->n_points == 0)
		return NULL;

	FT_Vector points[outline->n_points];
	FT_Vector *curr;
	float scale = grid_size / 64.0;
	int min_x, min_y, max_x, max_y;
	int i;
	for (i = 0; i < outline->n_points; i++) {
		curr = &outline->points[i];
		points[i].x = curr->x;
		points[i].y = curr->y;
		curr = &points[i];

		if (scale != 0 && scale != 1) {
			curr->x *= scale;
			curr->y *= scale;
		}

		curr->x += origin_x;
		curr->y = origin_y - curr->y;

		if (i == 0) {
			max_x = min_x = curr->x;
			max_y = min_y = curr->y;
			continue;
		}

		if (curr->x < min_x)
			min_x = curr->x;
		else if (curr->x > max_x)
			max_x = curr->x;

		if (curr->y < min_y)
			min_y = curr->y;
		else if (curr->y > max_y)
			max_y = curr->y;
	}

	int is_new_canvas = 0;
	if (canvas) {
		printf("outline: %d,%d %dx%d\n", min_x, min_y, max_x - min_x,
				max_y - min_y);
		if (min_x < 0 || max_x >= canvas->rect.width || min_y < 0
				|| max_y >= canvas->rect.height) {
			printf("overflow: %d,%d %d,%d\n", min_x, min_y, max_x, max_y);
			return NULL;
		}
		min_x = 0, min_y = 0;
//		min_x = 0, min_y = 200;
	} else {
		is_new_canvas = 1;
		canvas = malloc(sizeof(bitmap_t));
		if (!canvas)
			return NULL;

		int margin = 2;

		/* extra pixel for displaced_downsample */
		min_x -= extra_left(min_x, grid_size, margin);
		max_x += extra_right(max_x, grid_size, margin);

		/* padding */
		min_x = pad_floor(min_x, grid_size);
		min_y = pad_floor(min_y - margin, grid_size);
		/* plus one for real width and height */
		max_x = pad_ceil(max_x + 1, grid_size);
		max_y = pad_ceil(max_y + margin + 1, grid_size);

		rect_s rect = { 0, 0, max_x - min_x, max_y - min_y };
		if (!bitmap_init(canvas, &rect, 1)) {
			free(canvas);
			return NULL;
		}
//		printf("contour bitmap: %dx%d\n", rect.width, rect.height);
	}

#ifdef FONT_DEBUG
	char tag_mode = FT_CURVE_TAG_ON | FT_CURVE_TAG_CONIC | FT_CURVE_TAG_CUBIC;
	char *tags[] = {"CONIC", "ON", "CUBIC"};
	char buf[2];
	buf[1] = 0;
#endif

	int tag, j = 0;
	int contour_draws = 0;
	int is_contour_end = 0;
	FT_Vector *contour_start = NULL;
	FT_Vector *last_on = NULL, *last_conic = NULL;
	FT_Vector *cubic1 = NULL, *cubic2 = NULL;
	FT_Vector virtual_on;
	FT_Vector temp;
	for (i = 0; i < outline->n_points; i++) {
		tag = outline->tags[i];
		curr = &points[i];
		curr->x -= min_x;
		curr->y -= min_y;

		if (tag & FT_CURVE_TAG_HAS_SCANMODE)
			printf("FT_CURVE_TAG_HAS_SCANMODE\n");

#ifdef FONT_DEBUG
		buf[0] = i % 2 == 0 ? (char) ('A' + i / 2) : (char) ('a' + i / 2);
		printf("contour-%d %s(%d) %03ld,%03ld %c", j, tags[tag & tag_mode], tag,
				curr->x, curr->y, buf[0]);
#endif

		if (contour_start == NULL)
			contour_start = curr;
		is_contour_end = i == outline->contours[j];

		// http://www.freetype.org/freetype2/docs/glyphs/glyphs-6.html
		// Two successive ‘on’ points indicate a line segment joining them.
		// ...

		// http://freetype.org/freetype2/docs/reference/ft2-outline_processing.html#FT_Outline
		if (tag & FT_CURVE_TAG_ON) {
			if (last_on) {
#ifdef FONT_DEBUG
				printf(" |%s", last_conic ? "conic" : "line");
#endif
				if (last_conic) {
					font_draw_conic(canvas, last_on, last_conic, curr,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						font_slim_contour(canvas, last_on);
				} else if (cubic1 && cubic2) {
					font_draw_cubic(canvas, last_on, cubic1, cubic2, curr,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						font_slim_contour(canvas, last_on);
				} else {
					if (!is_same_point(last_on, curr)/*TBD*/) {
						font_draw_line(canvas, last_on, curr,
								contour_colors[contour_draws++ % 2]);
						if (contour_draws > 1)
							font_slim_contour(canvas, last_on);
					}
				}
			}

			if (is_contour_end) {
				// if (curr == contour_start) then just draw a point
				font_draw_line(canvas, curr, contour_start,
						contour_colors[contour_draws++ % 2]);
				if (contour_draws > 1)
					font_slim_contour(canvas, curr);
				// TODO test this
//				last_on = NULL;
//				last_conic = NULL;
//			} else {
//				last_on = curr;
			}

			last_on = curr, last_conic = NULL;
			cubic1 = NULL, cubic2 = NULL;
		} else if (tag & FT_CURVE_TAG_CUBIC) {
			if (cubic1)
				cubic2 = curr;
			else
				cubic1 = curr;
			if (is_contour_end && cubic1 && cubic2) {
				font_draw_cubic(canvas, last_on, cubic1, cubic2, contour_start,
						contour_colors[contour_draws++ % 2]);
				if (contour_draws > 1)
					font_slim_contour(canvas, last_on);
			}
		} else {
			if (last_conic) {
				// we got two successive conic ‘off’ points, create a virtual ‘on’ point inbetween them
				// save last_on before changing virtual_on because last_on can be a point to virtual_on
				if (last_on) {
					temp.x = last_on->x;
					temp.y = last_on->y;
					virtual_on.x = (last_conic->x + curr->x) / 2;
					virtual_on.y = (last_conic->y + curr->y) / 2;
					font_draw_conic(canvas, &temp, last_conic, &virtual_on,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						font_slim_contour(canvas, &temp);
#ifdef FONT_DEBUG
					printf(" |on/conic %ld %ld - %ld %ld", temp.x, temp.y,
							virtual_on.x, virtual_on.y);
#endif
					last_on = &virtual_on;
				}
			}
			if (is_contour_end) {
				if (last_on) {
					font_draw_conic(canvas, last_on, curr, contour_start,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						font_slim_contour(canvas, last_on);
				}
#ifdef FONT_DEBUG
				printf(" |on/conic %ld %ld - %ld %ld", last_on->x, last_on->y,
						contour_start->x, contour_start->y);
#endif
				// TODO test this
//				last_on = NULL;
//				last_conic = NULL;
//			} else {
//				last_conic = curr;
			}
			last_conic = curr;
		}

		if (is_contour_end) {
			font_slim_contour(canvas, contour_start);
			j++;
			contour_draws = 0;
			contour_start = NULL;
			last_on = NULL, last_conic = NULL;
			cubic1 = NULL, cubic2 = NULL;
#ifdef FONT_DEBUG
			printf(" *\n");
		} else {
			printf("\n");
#endif
		}

#ifdef LABEL_CONTOUR_POINTS
		rect_init(&rect, curr->x, curr->y, font->x_ppem_26_6 >> 6,
				font->ascender + font->descender);
		glyph_run_init(&grun, 0, 0);
		wchar_t *label = mbs2wcs(buf);
		text_draw(&bmp_canvas, label, font, &rect, &grun);
		free(label);
#endif
	}

	if (is_new_canvas) {
		canvas->rect.left = min_x;
		canvas->rect.top = min_y;
	}

	return canvas;
}

char font_outline2bitmap(FT_Outline *outline, FT_Bitmap *bitmap,
		FT_Bitmap *argb32, int grid_size, FT_Vector *offset) {
	bitmap_t *bmp = font_draw_contours(NULL, outline, 0, 0, grid_size);
	if (!bmp)
		return 0;

	int w = bmp->rect.width / grid_size;
	int h = bmp->rect.height / grid_size;

	offset->x = bmp->rect.left / grid_size;
	offset->y = bmp->rect.top / grid_size;
	bmp->rect.left = 0;
	bmp->rect.top = 0;

	bitmap->rows = h;
	bitmap->width = w * 3;
	bitmap->pitch = (bitmap->width + 3) & ~3;
	if (bitmap->buffer)
		free(bitmap->buffer);
	bitmap->buffer = calloc(1, bitmap->pitch * bitmap->rows);
	bitmap->num_grays = 256;
	bitmap->pixel_mode = FT_PIXEL_MODE_LCD;

	if (argb32) { /* for cairo */
		argb32->rows = h;
		argb32->width = w;
		argb32->pitch = w * 4;
		argb32->buffer = calloc(1, argb32->pitch * argb32->rows);
		argb32->num_grays = 256;
		argb32->pixel_mode = FT_PIXEL_MODE_LCD;
	}

	bitmap_t dst;
	rect_init(&dst.rect, 0, 0, w, h);
	dst.data = bitmap->buffer;
	dst.bytes_per_line = bitmap->pitch;
	dst.bytes_per_pixel = 3;

	font_fill_contours(&dst, w, h, bmp, 0, 0, grid_size, grid_size, 0);
	bitmap_free(bmp);

	return 1;
}

char font_render_glyph(FT_GlyphSlot glyph, int oversampling_rate,
		FT_Bitmap *argb32) {
	if (glyph->format != FT_GLYPH_FORMAT_OUTLINE)
		return 0;
	FT_Vector offset;
	font_outline2bitmap(&glyph->outline, &glyph->bitmap, argb32,
			oversampling_rate, &offset);
	glyph->bitmap_left = offset.x;
	glyph->bitmap_top = -offset.y;
	return 1;
}

void font_paint_raw(bitmap_t *dst, bitmap_t *src, int width, int height) {
	unsigned char *dst_line = dst->data;
	unsigned char *src_line = src->data;
	unsigned char *d, *s;
	int i, j;
	for (j = 0; j < src->rect.height && j < height;
			j++, (dst_line += dst->bytes_per_line, src_line +=
					src->bytes_per_line)) {
		d = dst_line;
		s = src_line;
		for (i = 0; i < src->rect.width && i < width; i++) {
			if (s[0] == contour_colors[0])
				d[2] = 0xff, d[1] = 0x00, d[0] = 0x00;
			else if (s[0] == contour_colors[1])
				d[2] = 0x00, d[1] = 0xff, d[0] = 0x00;
			else if (s[0] == slimming_color)
				d[2] = 0x00, d[1] = 0x00, d[0] = 0xff;
			else if (s[0] == filling_color)
				d[2] = 0x00, d[1] = 0x00, d[0] = 0x00;
			else if (s[0] == debug_color)
				d[2] = 0x00, d[1] = 0x00, d[0] = 0x00;
			d += dst->bytes_per_pixel;
			s++;
		}
	}
}

/* functions for bresenham_rasterizer */
//extern bitmap_t *bmp_raster;
//void setPixel(int x, int y) {
//	unsigned char *data = bmp_raster->data + y * bmp_raster->bytes_per_line
//			+ x * bmp_raster->bytes_per_pixel;
//	font_set_contour(data, contour_color);
//}
//void setPixelAA(int x, int y, int z) {
//	unsigned char *data = bmp_raster->data + y * bmp_raster->bytes_per_line
//			+ x * bmp_raster->bytes_per_pixel;
//}
//void setPixelColor(int x, int y, int z) {
//	unsigned char *data = bmp_raster->data + y * bmp_raster->bytes_per_line
//			+ x * bmp_raster->bytes_per_pixel;
//}
//
//	data[0] = 0x00;
//	data[1] = 0x00;
//	data[2] = 0xff;
//	if (contour_color == &color_r)
//		data[2] = 0xff;
//	else if (contour_color == &color_g)
//		data[1] = 0xff;
//	else if (contour_color == &color_b)
//		data[0] = 0xff;
//data[0] = 0x00;
//data[1] = 0x00;
//if (color == &color_r)
//	data[2] = 0xff;
//else
//	data[2] = 0x00;
//			// plot non-zero slope line
//			// http://jayurbain.com/msoe/cs421/Bresenham.pdf
//			// the simplest algorithm: DDA (digital differential analyzer)
//			double slope = (p1->y - p0->y) / (double) (p1->x - p0->x);
//			y = p0->y;
//			double y_ = y;
//			unsigned char *d;
//			for (x = p0->x; x <= p1->x; x++) {
//				d = data + y * bmp_raster->bytes_per_line
//						+ x * bmp_raster->bytes_per_pixel;
//				d[0] = 0x00;
//				d[1] = 0x00;
//				d[2] = 0xff; // red
//				y_ += slope;
//				y = y_ + 0.5;
//			}
//			slope = (p1->y - p0->y) / (double) (p1->x - p0->x);
//			x = p0->x;
//			double x_ = x;
//inline int font_contour_around(unsigned char *s, int bytes_per_line,
//		char at_first_line, char at_last_line) {
//	int bytes_per_pixel = 1;
//	int above_on = 0, below_on = 0;
//	if (!at_first_line) {
//		unsigned char *a = s - bytes_per_line;
//		above_on = font_contour_on(a - bytes_per_pixel) || font_contour_on(a)
//				|| font_contour_on(a + bytes_per_pixel);
//	}
//	if (!at_last_line) {
//		unsigned char *b = s + bytes_per_line;
//		below_on = font_contour_on(b - bytes_per_pixel) || font_contour_on(b)
//				|| font_contour_on(b + bytes_per_pixel);
//	}
//	if (above_on && below_on)
//		return 0; // return PREV_ON_ABOVE | PREV_ON_BELOW;
//	return above_on ? PREV_ON_ABOVE : (below_on ? PREV_ON_BELOW : 0);
//}
