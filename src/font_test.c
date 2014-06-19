#include "wl_x11.h"
#include "wl_text.h"
#include "wl_raster.h"
#include "wl_gamma.h"
#include "wl_filter.h"
#include "wl_subpixel_filter.h"
#include <locale.h>
#include <stdint.h>

wchar_t *text = L"Wind"; // meyLinux // abdfghpqy// Windows Steve 中文字体ɒɔ

int font_height = 15;

// 18 30
//int grid_size = 18;
int grid_size = 30;

#define DRAW_GRIDLINES
//#define LABEL_CONTOUR_POINTS
//#define FILL_CONTOURS 0
#define FILL_CONTOURS 1
//#define BLACK_ON_WHITE 0
#define BLACK_ON_WHITE 1 /* or WHITE_ON_BLACK */
float gamma_value = 2.2;
int subpixel_rendering = 1;

//FT_ULong load_flags = FT_LOAD_NO_HINTING;
//FT_ULong load_flags = FT_LOAD_NO_AUTOHINT;
//FT_ULong load_flags = FT_LOAD_FORCE_AUTOHINT;
FT_ULong load_flags = FT_LOAD_TARGET_LIGHT;

//unsigned char filter[] = { 0x18, 0x1C, 0x97, 0x1C, 0x18 }; // Mac OS X 10.8
//float fringe_filter[] = { 0.119, 0.370, 0.511, 0.370, 0.119 }; // mitchell_filter
//float fringe_filter[] = { 0.090, 0.373, 0.536, 0.373, 0.090 }; // catmull_rom_filter
//float fringe_filter[] = { -0.106, 0.244, 0.726, 0.244, -0.106 };
//float fringe_filter[] = { 0.059, 0.235, 0.412, 0.235, 0.059 };
//float fringe_filter[] = { 0., 1. / 4., 2. / 4., 1. / 4., 0. };
//float fringe_filter[] = { 1. / 17., 4. / 17., 7. / 17., 4. / 17., 1. / 17. };
float fringe_filter[] = { 1. / 16., 5. / 16., 10. / 16., 5. / 16., 1. / 16. };

#define FIR5_FILTER 0
#define CUSTOME_FILTER 1
#define DISPLACED_FILTER 2
#define DISPLACED_WEIGHTED 3  /* displaced filter with oversampling by RGB luminance weights */
int subpixel_filter_type = DISPLACED_WEIGHTED;

// A filter design algorithm for subpixel rendering on matrix displays (2007)
//float displaced_filter_weights[] = { 0.33, 0.34, 0.33 };
// Optimizing subpixel rendering using a perceptual metric (2011)
float displaced_filter_weights[] = { 0.3, 0.4, 0.3 };

//unsigned char filter[] = { 15, 60, 105, 60, 15 };
//unsigned char filter[] = { 30, 60, 85, 60, 30 };
//	unsigned char filter[] = {0, 0, 255, 0, 0};
//	unsigned char filter[] = {28, 56, 85, 56, 28};

/* color component-value pair */
unsigned char contour_colors[3] = { 1, 2 };
unsigned char slim_color = 4;
unsigned char filling_color = 0xff;
unsigned char debug_color = 0x80;
unsigned char grid_color = 0x00;

bitmap_t *bmp_raster = NULL; /* for bresenham_rasterizer */ // TODO free

font_conf_t font;
glyph_run_t grun;
rect_s rect;

void text_compare() {
	char s[1024];
	char *flagl[] = { "FT_LOAD_DEFAULT", "FT_LOAD_NO_HINTING",
			"FT_LOAD_NO_AUTOHINT", "FT_LOAD_FORCE_AUTOHINT", };
	int flags[] = { FT_LOAD_DEFAULT, FT_LOAD_NO_HINTING, FT_LOAD_NO_AUTOHINT,
	FT_LOAD_FORCE_AUTOHINT };
	char *model[] = { "FT_RENDER_MODE_MONO", "FT_RENDER_MODE_NORMAL",
			"FT_RENDER_MODE_LIGHT", "FT_RENDER_MODE_LCD" };
	FT_Render_Mode modes[] = { FT_RENDER_MODE_MONO, FT_RENDER_MODE_NORMAL,
			FT_RENDER_MODE_LIGHT, FT_RENDER_MODE_LCD };
	int i, j;
	for (j = 3; j < sizeof(modes) / sizeof(modes[0]); j++) {
		for (i = 4; i < sizeof(flags) / sizeof(flags[0]); i++) {
			font.load_flags = flags[i];
			font.render_mode = modes[j];
			grun.curr_pos = 0;
			grun.offset_x_26_6 = 0;
			text_draw(&bmp_canvas, text, &font, &rect, &grun);

//			grun.curr_pos = 0;
//			grun.offset_x_26_6 = 0;
//			strcpy(s, model[j]);
//			strcat(s, "/");
//			strcat(s, flagl[i]);
//			wchar_t *w = mbs2wcs(s);
//			text_draw(&bmp_canvas, w, &font, &rect, &grun);
//			free(w);
		}
	}
}

inline int is_same_point(FT_Vector *p0, FT_Vector *p1) {
	return p0->x == p1->x && p0->y == p1->y;
}

inline void bitmap_init(bitmap_t *bmp, rect_s *rect, int bytes_per_pixel) {
	rect_init(&bmp->rect, rect->left, rect->top, rect->width, rect->height);
	bmp->bytes_per_pixel = bytes_per_pixel;
	bmp->bytes_per_line = bmp->bytes_per_pixel * rect->width;
	bmp->data = calloc(1, bmp->bytes_per_line * rect->height);
}

void bitmap_shift(bitmap_t *bmp, int shift_x, int shift_y) {
	bmp->rect.left += shift_x;
	bmp->rect.width -= shift_x;
	bmp->rect.top += shift_y;
	bmp->rect.height -= shift_y;
	bmp->data += shift_y * bmp->bytes_per_line + shift_x * bmp->bytes_per_pixel;
}

inline void raster_set_contour(unsigned char *d, unsigned char color) {
	if (d[0] != slim_color)
		d[0] = color;
}

inline void raster_set_pixel(bitmap_t *r, int x, int y, unsigned char color) {
	unsigned char *d = r->data + y * r->bytes_per_line + x * r->bytes_per_pixel;
	d[0] = color;
}

inline int raster_on_contour(unsigned char *s) {
	return s[0] == contour_colors[0] || s[0] == contour_colors[1];
}

inline void raster_copy_contour(bitmap_t *dst, bitmap_t *src, int width,
		int height) {
	unsigned char *dst_line = dst->data;
	unsigned char *src_line = src->data;
	unsigned char *d, *s;
	int i, j;
	for (j = 0; j < src->rect.height && j < height;
			++j, (dst_line += dst->bytes_per_line, src_line +=
					src->bytes_per_line)) {
		d = dst_line;
		s = src_line;
		for (i = 0; i < src->rect.width && i < width; ++i) {
			if (s[0] == contour_colors[0])
				d[2] = 0xff, d[1] = 0x00, d[0] = 0x00;
			else if (s[0] == contour_colors[1])
				d[2] = 0x00, d[1] = 0xff, d[0] = 0x00;
			else if (s[0] == slim_color)
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

inline int raster_slim_contour(bitmap_t *canvas, FT_Vector *p0) {
	if (p0->x < 0 || p0->x >= canvas->rect.width || p0->y < 0
			|| p0->y >= canvas->rect.height)
		return 0;

	unsigned char *s = canvas->data + p0->y * canvas->bytes_per_line
			+ p0->x * canvas->bytes_per_pixel;
	unsigned char *t = s - canvas->bytes_per_line;

	// line above
	int a = raster_on_contour(t);
	int al = raster_on_contour(t - canvas->bytes_per_pixel);
	int ar = raster_on_contour(t + canvas->bytes_per_pixel);

	// line below
	t = s + canvas->bytes_per_line;
	int b = raster_on_contour(t);
	int bl = raster_on_contour(t - canvas->bytes_per_pixel);
	int br = raster_on_contour(t + canvas->bytes_per_pixel);

	// left and right
	int l = raster_on_contour(s - canvas->bytes_per_pixel);
	int r = raster_on_contour(s + canvas->bytes_per_pixel);

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
				s[0] = slim_color;
				FT_Vector p = *p0;
				p.y++;
				raster_slim_contour(canvas, &p);
				if (bl) {
					p.x--;
					raster_slim_contour(canvas, &p);
				} else if (br) {
					p.x++;
					raster_slim_contour(canvas, &p);
				}
			}
			/* above: ---    ---
			 *        -x- => ---
			 * below: -x-    -x-
			 */
			else if (!(l || r)) {
				s[0] = slim_color;
				FT_Vector p = *p0;
				p.y++;
				raster_slim_contour(canvas, &p);
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
				s[0] = slim_color;
				FT_Vector p = *p0;
				p.y--;
				raster_slim_contour(canvas, &p);
				if (al) {
					p.x--;
					raster_slim_contour(canvas, &p);
				} else if (ar) {
					p.x++;
					raster_slim_contour(canvas, &p);
				}
			} else if (!(l || r)) {
				s[0] = slim_color;
				FT_Vector p = *p0;
				p.y--;
				raster_slim_contour(canvas, &p);
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
		s[0] = slim_color;
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

#define PREV_ON 1
#define PREV_ON_ABOVE 2
#define PREV_ON_BELOW 4
#define BEFOR_ON_FILL 8

inline int raster_contour_around(unsigned char *s, int bytes_per_line) {
	int bytes_per_pixel = 1;
	unsigned char *a = s - bytes_per_line;
	unsigned char *b = s + bytes_per_line;
	int above_on = raster_on_contour(a - bytes_per_pixel)
			|| raster_on_contour(a) || raster_on_contour(a + bytes_per_pixel);
	int below_on = raster_on_contour(b - bytes_per_pixel)
			|| raster_on_contour(b) || raster_on_contour(b + bytes_per_pixel);
	if (above_on && below_on)
		return 0; // return PREV_ON_ABOVE | PREV_ON_BELOW;
	if (above_on)
		return PREV_ON_ABOVE;
	if (below_on)
		return PREV_ON_BELOW;
	return 0;
}

// http://en.wikipedia.org/wiki/Rasterisation#Scan_conversion
// http://en.wikipedia.org/wiki/Scanline_algorithm
void raster_fill_contours(bitmap_t *dst, bitmap_t *src, int x, int y, int dst_w,
		int dst_h, int grid_width, int grid_height) {
	int i, j, k, l, m;

	int dst_pixel_size = dst->bytes_per_pixel;
	int dst_line_size = dst->bytes_per_line;
	unsigned char *dst_line = dst->data;

	int src_pixel_size = 1; // src->bytes_per_pixel;
	int src_line_size = src->bytes_per_line;
	unsigned char *src_line_outer = src->data + y * src_line_size
			+ x * src_pixel_size;

	unsigned char *src_line, *s_outer, *s, *d;
	int src_line_incr = src_line_size * grid_width;
	int src_grid_incr = src_pixel_size * grid_width;

	unsigned char linear2srgb[256];
	gamma_linear2srgb(linear2srgb, gamma_value);

	const unsigned grid_area = grid_width * grid_height;
	float grid_weight;

	char rgb_weighted = subpixel_filter_type == DISPLACED_WEIGHTED;

	// subpixel rendering
	int subgrid_count; // how many subpixels we want from a grid
	int subgrid_index;
//	unsigned *subgrid_widths = NULL;
	unsigned *subgrid_areas = NULL;
	unsigned *subgrid_limits = NULL;
	float *subgrid_weights = NULL;
	unsigned char *subpixel_line = NULL; // of dst
	unsigned subpixel_line_size;
	if (subpixel_rendering) {
		if (rgb_weighted) {
			subgrid_count = grid_width / (6 + 3 + 1) * 3; // each subgrid (dst pixel) has 6R, 3G, 1B
			subpixel_line_size = dst_w * subgrid_count + 3 * 2; // add 2 more pixels
		} else if (subpixel_filter_type == DISPLACED_FILTER) {
			subgrid_count = 9; // we need 3 subgrids
			subpixel_line_size = dst_w * subgrid_count + 3 * 1; // add 1 more pixels
		} else {
			subgrid_count = 3;
			subpixel_line_size = dst_w * subgrid_count + 4; // left and right margin of 2 subpixels each
		}

		subgrid_limits = malloc(sizeof(unsigned) * subgrid_count);
		subgrid_areas = malloc(sizeof(int) * subgrid_count);
		subgrid_weights = malloc(sizeof(double) * subgrid_count);
		subpixel_line = malloc(subpixel_line_size);
		printf("subgrid_widths:");
		j = 0;
		for (i = 0; i < subgrid_count; i++) {
			if (rgb_weighted)
				k = i % 3 == 0 ? 3 : (i % 3 == 1 ? 6 : 1);
			else
				k = (grid_width - j) / (subgrid_count - i);
			j += k;
			subgrid_areas[i] = k * grid_height;
			subgrid_limits[i] = j;
			printf(" %d", k);
		}
		printf("\nsubgrid_limits:");
		for (i = 0; i < subgrid_count; i++)
			printf(" %d", subgrid_limits[i]);
		printf("\n");
	}

	char *prev_on = malloc(grid_height);
	char *fill_flags = malloc(grid_height);
	for (j = 0; j < dst_h; ++j, (dst_line += dst_line_size, src_line_outer +=
			src_line_incr)) {
		d = dst_line;
		s_outer = src_line_outer;

		memset(prev_on, 0, grid_height);
		memset(fill_flags, 0, grid_height);
		if (subpixel_rendering)
			memset(subpixel_line, 0, subpixel_line_size);

		for (i = 0; i < dst_w; ++i, (s_outer += src_grid_incr)) {
			src_line = s_outer;

			// calculate coverage of grid (i,j) (ie. the dst pixel (i,j))
			if (subpixel_rendering)
				memset(subgrid_weights, 0, sizeof(float) * subgrid_count);
			else
				grid_weight = 0;

			for (y = 0; y < grid_height; y++, (src_line += src_line_size)) {
				s = src_line;

				subgrid_index = 0;
				for (x = 0; x < grid_width; x++, (s += src_pixel_size)) {
					/* convex (up and down) curves: \_/ and /`\
					 *     *--a (change on a)/(recover to BEFOR_ON_FILL)
					 *    /    \
					 *   /  *b  \ (change on b?)/(recover to BEFOR_ON_FILL)
					 *  /  /  \  \
					 * *--*    *--*
					 */
//					if (i == 5 && j == 7 && x == 2 && y == 5)
//						s[0] = debug_color;
					if (raster_on_contour(s)) {
						if (!prev_on[y]) {
							//  (off) on
							prev_on[y] = PREV_ON
									| (fill_flags[y] ? BEFOR_ON_FILL : 0);
							int around_on = raster_contour_around(s,
									src_line_size);
							prev_on[y] |= around_on;
							fill_flags[y] = !fill_flags[y];
						} //EL  (on)  on
					} else {
						if (prev_on[y]) {
							//  (on) off
							int around_on = raster_contour_around(
									s - src_pixel_size, src_line_size);
							if (around_on && (prev_on[y] & around_on))
								fill_flags[y] = prev_on[y] & BEFOR_ON_FILL;
							prev_on[y] = 0;
						} //EL (off) off
					}

					if (fill_flags[y]) {
						if (s[0] == 0)
							s[0] = filling_color;
					}

					// finished filling current src pixel

					if (x >= subgrid_limits[subgrid_index])
						subgrid_index++;

					if (s[0] == 0)
						continue;

					// accumulate grid weight
					if (subpixel_rendering)
						subgrid_weights[subgrid_index]++;
					else
						grid_weight++;
				}
			}

			// grid weight ready

			if (subpixel_rendering) {
				for (l = 0; l < subgrid_count; l++) {
					if (subgrid_weights[l] == 0)
						continue;

					grid_weight = subgrid_weights[l] / subgrid_areas[l];
					if (grid_weight > 1)
						perror("grid_weight > 1");
//					printf("%f %.0f/%d\n", grid_weight, subgrid_weights[k], subgrid_areas[k]);

					m = i * subgrid_count + l; // subpixel index
					if (subpixel_filter_type == DISPLACED_FILTER
							|| rgb_weighted) {
						subpixel_line[m] = grid_weight * 255;
					} else if (subpixel_filter_type == CUSTOME_FILTER) {
						subpixel_line[m + 2] = grid_weight * 255;
					} else {
						for (k = 0; k < 5; k++) {
							float f = subpixel_line[m + k]
									+ grid_weight * fringe_filter[k] * 255;
							if (f < 0)
								f = 0;
							subpixel_line[m + k] = f > 255 ? 255 : f;
						}
					}
				}
//				d[0] = 0xff * subgrid_weights[2], d[1] = 0xff * subgrid_weights[1], d[2] =
//						0xff * subgrid_weights[0];
			} else if (grid_weight > 0) {
				grid_weight = grid_weight / grid_area;
				if (BLACK_ON_WHITE)
					grid_weight = 1 - grid_weight;
				d[0] = 0xff * grid_weight;
				d[1] = 0xff * grid_weight;
				d[2] = 0xff * grid_weight;
//				printf("%d %d = %.0f/%.0f\n", x, y, grid_weight, grid_area);
				d += dst_pixel_size;
			}
		}

		if (subpixel_rendering) {
			if (subpixel_filter_type == DISPLACED_FILTER || rgb_weighted) {
				displaced_downsample(d, subpixel_line, dst_w,
						displaced_filter_weights);
				i = 0;
				while (i++ < dst_w) {
//					for (k = 0; k < dst_pixel_size; k++)
//						d[k] = linear2srgb[0xff - d[k]];
//					assert(dst_pixel_size >= 3);
					k = d[0];
					d[0] = linear2srgb[0xff - d[2]];
					d[1] = linear2srgb[0xff - d[1]];
					d[2] = linear2srgb[0xff - k];
					d += dst_pixel_size;
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
					d[2] = linear2srgb[0xff - subpixel_line[i++]];
					d[1] = linear2srgb[0xff - subpixel_line[i++]];
					d[0] = linear2srgb[0xff - subpixel_line[i++]];
					d += dst_pixel_size;
				}
			}
		}
	}

	free(prev_on);
	free(fill_flags);
	if (subgrid_limits)
		free(subgrid_limits);
	if (subgrid_weights)
		free(subgrid_weights);
	if (subpixel_line)
		free(subpixel_line);
}

void raster_draw_contours(bitmap_t *canvas, FT_Outline *outline, int origin_x,
		int origin_y, float scale) {
	FT_Vector points[outline->n_points];
	FT_Vector *curr;
	int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
	int i, j = 0;
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

		if (curr->x > max_x)
			max_x = curr->x;
		else if (curr->x < min_x)
			min_x = curr->x;
		if (curr->y > max_y)
			max_y = curr->y;
		else if (curr->y < min_y)
			min_y = curr->y;
	}

	if (min_x < 0 || max_x >= canvas->rect.width || min_y < 0
			|| max_y >= canvas->rect.height) {
		printf("overflow: %d,%d %d,%d\n", min_x, max_x, min_y, max_y);
		return;
	}

#ifdef DEBUG
	char tag_mode = FT_CURVE_TAG_ON | FT_CURVE_TAG_CONIC | FT_CURVE_TAG_CUBIC;
	char *tags[] = {"CONIC", "ON", "CUBIC"};
	char buf[2];
	buf[1] = 0;
#endif

	int tag;
	FT_Vector *last_on = NULL;
	FT_Vector *last_conic = NULL;
	FT_Vector virtual_on;
	FT_Vector temp;
	FT_Vector *contour_start = NULL;
	int is_contour_end = 0;
	int contour_draws = 0;
	for (i = 0; i < outline->n_points; i++) {
		curr = &points[i];
		tag = outline->tags[i];

		if (tag & FT_CURVE_TAG_HAS_SCANMODE)
			printf("FT_CURVE_TAG_HAS_SCANMODE\n");

#ifdef DEBUG
		buf[0] = i % 2 == 0 ? (char) ('A' + i / 2) : (char) ('a' + i / 2);
		printf("C%d %s(%d) %03ld,%03ld %c", j, tags[tag & tag_mode], tag,
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
#ifdef DEBUG
				printf(" |%s", last_conic ? "conic" : "line");
#endif
				if (last_conic) {
					draw_conic(canvas, last_on, last_conic, curr,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						raster_slim_contour(canvas, last_on);
				} else {
					if (!is_same_point(last_on, curr)/*TBD*/) {
						draw_line(canvas, last_on, curr,
								contour_colors[contour_draws++ % 2]);
						if (contour_draws > 1)
							raster_slim_contour(canvas, last_on);
					}
				}
			}

			if (is_contour_end) {
				// if (curr == contour_start) then just draw a point
				draw_line(canvas, curr, contour_start,
						contour_colors[contour_draws++ % 2]);
				if (contour_draws > 1)
					raster_slim_contour(canvas, curr);
				last_on = NULL;
				last_conic = NULL;
			} else {
				last_on = curr;
			}

			last_conic = NULL; // clear last_conic when we see an `on'
		} else if (tag & FT_CURVE_TAG_CUBIC) {
		} else {
			if (last_conic) {
				// we got two successive conic ‘off’ points, create a virtual ‘on’ point inbetween them
				// save last_on before changing virtual_on because last_on can be a point to virtual_on
				if (last_on) {
					temp.x = last_on->x;
					temp.y = last_on->y;
					virtual_on.x = (last_conic->x + curr->x) / 2;
					virtual_on.y = (last_conic->y + curr->y) / 2;
					draw_conic(canvas, &temp, last_conic, &virtual_on,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						raster_slim_contour(canvas, &temp);
#ifdef DEBUG
					printf(" |on/conic %ld %ld - %ld %ld", temp.x, temp.y,
							virtual_on.x, virtual_on.y);
#endif
				}
				last_on = &virtual_on;
			}
			if (is_contour_end) {
				if (last_on) {
					draw_conic(canvas, last_on, curr, contour_start,
							contour_colors[contour_draws++ % 2]);
					if (contour_draws > 1)
						raster_slim_contour(canvas, last_on);
				}
#ifdef DEBUG
				printf(" |on/conic %ld %ld - %ld %ld", last_on->x, last_on->y,
						contour_start->x, contour_start->y);
#endif
				last_on = NULL;
				last_conic = NULL;
			} else {
				last_conic = curr;
			}
		}

		if (is_contour_end) {
			raster_slim_contour(canvas, contour_start);
			j++;
			contour_start = NULL;
			contour_draws = 0;
#ifdef DEBUG
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
}

int x11_processor(XEvent *event) {
	switch (event->type) {
	case Expose:
		break;
	case ConfigureNotify:
		break;
	case ResizeRequest:
		bmp_output.rect.width = event->xresizerequest.width;
		bmp_output.rect.height = event->xresizerequest.height;
		bmp_canvas.rect.width = event->xresizerequest.width;
		bmp_canvas.rect.height = event->xresizerequest.height;
		printf("resize: %dx%d\n", bmp_canvas.rect.width,
				bmp_canvas.rect.height);
		break;
	case MapNotify:
		printf("mapnot: %dx%d\n", bmp_canvas.rect.width,
				bmp_canvas.rect.height);

		window_image_create(&bmp_output, &img_output, 0, 0xff);
		bmp_canvas = bmp_output;
		if (!bmp_raster)
			bmp_raster = malloc(sizeof(bitmap_t));
		bitmap_init(bmp_raster, &bmp_canvas.rect, 1);

		if (0) {
			glyph_run_init(&grun, 0, 0);
			rect.left = 10;
			rect.top = 100;
			rect.height = bmp_canvas.rect.height - -rect.top * 2;
			rect.width = bmp_canvas.rect.width - rect.left * 2;
			text_draw(&bmp_canvas, text, &font, &rect, &grun);

			text_compare();
			FT_Face face = font.face;
			FTC_ScalerRec scaler = font.scaler;
			font.face = font.fallback;
			font.scaler = font.scaler_fallback;
			text_compare();
			font.face = face;
			font.scaler = scaler;
		} else {
			float grid_scale = grid_size / 64.0;
			int offset_x = 20;
			int offset_y = 20;
			int origin_y = offset_y + font.ascender * grid_size;

			int i, advance_x = 0;
			for (i = 0; i < wcslen(text); i++) {
				FT_Glyph glyph = glyph_get(text[i], &font, load_flags);
				if (glyph->format != FT_GLYPH_FORMAT_OUTLINE)
					continue;

				FT_OutlineGlyph o = (FT_OutlineGlyph) glyph;
				FT_Outline *outline = &o->outline;

				raster_draw_contours(bmp_raster, outline, offset_x + advance_x,
						origin_y, grid_scale);
				printf("`%lc' #contour=%d #point=%d\n", text[i],
						outline->n_contours, outline->n_points);

				int advance = glyph->advance.x >> 10;
				advance_x += advance * grid_scale;
			}
//			raster_set_pixel(bmp_raster, 60, 172, debug_color);

			if (advance_x % grid_size > 0)
				advance_x = (advance_x / grid_size + 1) * grid_size;

			if (FILL_CONTOURS) {
//				bitmap_t sub = *bmp_raster;
				bitmap_t sub = bmp_canvas;
				bitmap_shift(&sub, offset_x + advance_x + 20, offset_y);
				raster_fill_contours(&sub, bmp_raster, offset_x, offset_y,
						advance_x / grid_size, font.ascender + font.descender,
						grid_size, grid_size);
			}

			if (1) {
				unsigned char weights[] = { 0, 0, 0xff, 0, 0 };
				FT_Library_SetLcdFilterWeights(ft_library, weights);
				font.load_flags = load_flags;
				font.render_mode = FT_RENDER_MODE_LCD;
				font.scaler.height = font_height;
				font.scaler.width = font.scaler.height * 3;
				glyph_run_init(&grun, 0, 0);
				rect_init(&rect, offset_x + advance_x + 20, offset_y + 100, 100,
						100);
				text_draw(&bmp_canvas, text, &font, &rect, &grun);
			}

			int max_x = offset_x + advance_x;
			int max_y = origin_y + font.descender * grid_size;
			if (max_x > bmp_canvas.rect.width || max_y > bmp_canvas.rect.height)
				perror("text overflow");

			printf("`%ls' rect: (%d,%d) (%dx%d)/(%.1fx%.1f) font %d+%d\n", text,
					offset_x, offset_y, max_x - offset_x, max_y - offset_y,
					(max_x - offset_x) / (double) grid_size,
					(max_y - offset_y) / (double) grid_size, font.ascender,
					font.descender);
			printf("grid size: %d\n", grid_size);

#ifdef DRAW_GRIDLINES
			int x, y;
			FT_Vector p0, p1;

			// draw horizontal grid lines
			p0.x = offset_x, p1.x = max_x;
			for (y = offset_y; y <= max_y; y += grid_size) {
				p0.y = y, p1.y = y;
				draw_line(&bmp_canvas, &p0, &p1, grid_color);
			}

			// draw vertical grid lines
			p0.y = offset_y, p1.y = max_y;
			for (x = offset_x; x <= max_x; x += grid_size) {
				p0.x = x, p1.x = x;
				draw_line(&bmp_canvas, &p0, &p1, grid_color);
			}
#endif

			raster_copy_contour(&bmp_canvas, bmp_raster, max_x, max_y);
		}
		window_repaint(NULL);
		break;
	case ButtonPress:
		break;
	case KeyPress:
		break;
	}
	return 1;
}

int main(int argc, char *argv[]) {
	setlocale(LC_CTYPE, "en_US.UTF-8");

//	int i;
//	for (i = 0; i < 5; i++)
//		subpixel_filter[i] = filter[i];

//	IPA fonts: CharisSIL-R.ttf DoulosSIL-R.ttf LinLibertine_R.otf
	freetype_init(2, 2, 10240 * 1024, 1);
//	freetype_font("fonts/XinGothic.otf", "fonts/方正兰亭细黑_GBK.ttf");
	freetype_font("/home/pxlogpx/.fonts/Lucida Grande.ttf", 0,
			"fonts/arial.ttf", 0);
//	freetype_font("/home/pxlogpx/.fonts/_H_Menlo.ttc", 1, "fonts/arial.ttf", 0);
//	freetype_font("/home/pxlogpx/.fonts/_H_HelveticaNeue.ttc", 4,
//			"fonts/arial.ttf", 0);

	font_init(&font, font_height, 5);

	x11_init(1500, 600);

	x11_set_title("Font Rendering");
	x11_show_window(0, x11_processor, NULL, NULL, 0);

	x11_free();
	freetype_free();

	return EXIT_SUCCESS;
}

//if (0 && c == L'l') {
////		FT_Vector p0 = { 176, 260 };//{ 181, 260 };
////		FT_Vector p1 = { 176, 075 };//{ 181, 075 };
////		FT_Vector p2 = { 200, 075 };//{ 204, 075 };
////		FT_Vector p3 = { 200, 260 };//{ 204, 260 };
//	FT_Vector p0 = { 196 - 6 - 3, 260 };
//	FT_Vector p1 = { 196 - 6 - 3, 075 };
//	FT_Vector p2 = { 196 + 5 + 8, 075 };
//	FT_Vector p3 = { 196 + 5 + 8, 260 };
//	// grid of (180, 196)
//	draw_line(bmp_raster, &p0, &p1, contour_colors[0]);
//	draw_line(bmp_raster, &p1, &p2, contour_colors[0]);
//	draw_line(bmp_raster, &p2, &p3, contour_colors[0]);
//	draw_line(bmp_raster, &p3, &p0, contour_colors[0]);
//	return;
//}
