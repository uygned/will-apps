#include "wl_x11.h"
#include "wl_text.h"
#include "bresenham_rasterizer.h"
#include <locale.h>
#include <stdint.h>

wchar_t *test = L"B"; // Windows Steve 中文字体ɒɔ
font_conf_t font;
glyph_run_t grun;
rect_s rect;
int font_height = 10;

color_t color_r = { 0xff, 0x00, 0x00 };
color_t color_g = { 0x00, 0xff, 0x00 };
color_t color_b = { 0x00, 0x00, 0xff };
color_t *contour_color = &color_r;

// functions for bresenham_rasterizer
void setPixel(int x, int y) {
	bitmap_t *canvas = &bmp_canvas;
	unsigned char *data = canvas->data + y * canvas->bytes_per_line
			+ x * canvas->bytes_per_pixel;
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	if (contour_color == &color_r)
		data[2] = 0xff;
	else if (contour_color == &color_g)
		data[1] = 0xff;
	else if (contour_color == &color_b)
		data[0] = 0xff;
}

void setPixelAA(int x, int y, int z) {
	bitmap_t *canvas = &bmp_canvas;
	unsigned char *data = canvas->data + y * canvas->bytes_per_line
			+ x * canvas->bytes_per_pixel;
//	data[0] = 0x00;
//	data[1] = 0x00;
//	data[2] = 0xff;
}

void setPixelColor(int x, int y, int z) {
	bitmap_t *canvas = &bmp_canvas;
	unsigned char *data = canvas->data + y * canvas->bytes_per_line
			+ x * canvas->bytes_per_pixel;
//	data[0] = 0x00;
//	data[1] = 0x00;
//	data[2] = 0xff;
}

void draw_line(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1) {
	int x, y;
//	unsigned char *data = canvas->data;
	unsigned char *data = canvas->data
			+ canvas->rect.top * canvas->bytes_per_line
			+ canvas->rect.left * canvas->bytes_per_pixel;
	if (p0->x == p1->x) {
		if (p0->y == p1->y) {
			// draw one pixel
			data += p0->y * canvas->bytes_per_line
					+ p0->x * canvas->bytes_per_pixel;
			data[0] = 0x00;
			data[1] = 0x00;
			data[2] = 0xff; // red
		} else {
			// draw vertical line
			if (p0->y > p1->y) {
				FT_Vector *p = p0;
				p0 = p1;
				p1 = p;
			}

			data += p0->y * canvas->bytes_per_line
					+ p0->x * canvas->bytes_per_pixel;
			for (y = p0->y; y < p1->y; y++) {
				data[0] = 0x00;
				data[1] = 0x00;
				data[2] = 0xff; // red
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
			for (x = p0->x; x < p1->x; x++) {
				data[0] = 0x00;
				data[1] = 0x00;
				data[2] = 0xff; // red
				data += canvas->bytes_per_pixel;
			}
		} else {
			unsigned char *d;
			double slop = (p1->y - p0->y) / (double) (p1->x - p0->x);
			for (x = p0->x; x < p1->x; x++) {
				int y = p0->y + (x - p0->x) * slop;
				d = data + y * canvas->bytes_per_line
						+ x * canvas->bytes_per_pixel;
				d[0] = 0x00;
				d[1] = 0x00;
				d[2] = 0xff; // red
			}
		}
	}
}

void draw_conic(bitmap_t *canvas, FT_Vector *p0, FT_Vector *p1/*conic*/,
		FT_Vector *p2) {
	contour_color = &color_g;
	plotQuadBezier(p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
	return;
}

void outline_draw(bitmap_t *canvas, wchar_t c, font_conf_t *font, char fallback) {
	FT_Glyph glyph;

	FT_Face face = font->face;
	FTC_Scaler scaler = &font->scaler;

	FT_UInt glyph_index = FT_Get_Char_Index(face, (FT_ULong) c);
	if (fallback) {
		face = font->fallback;
		scaler = &font->scaler_fallback;
		glyph_index = FT_Get_Char_Index(face, (FT_ULong) c);
	}

	if (glyph_index == 0)
		perror("glyph_index");

	if (FTC_ImageCache_LookupScaler(ft_cache, scaler, font->load_flags,
			glyph_index, &glyph, NULL))
		perror("FTC_ImageCache_LookupScaler");

	FT_OutlineGlyph og = (FT_OutlineGlyph) glyph;
	FT_Outline *outline = &og->outline;

	char buf[2];
	buf[1] = 0;

	int i, j = 0;
	int offset_x = 2;
	int offset_y = 2;
	int draw = 1;
	for (i = 0; i < outline->n_points; i++) {
		FT_Vector *curr = &outline->points[i];
		curr->x += 20;
		curr->y += 100;
		curr->y = canvas->rect.height - curr->y;
//		if (curr->x < 1 || curr->y < 1 || curr->x >= canvas->rect.width
//				|| curr->y >= canvas->rect.height) {
//			printf("overflow: %ld,%ld\n", curr->x, curr->y);
//			draw = 0;
//			break;
//		}
	}

	if (!draw)
		return;

	FT_Vector *last_on = NULL;
	FT_Vector *last_conic = NULL;
	FT_Vector virtual_on;
	FT_Vector temp;

	char *tags[] = { "conic", "on", "cubic" };
	FT_Vector *contour_start = NULL;
	int is_contour_end = 0;
	for (i = 0; i < outline->n_points; i++) {
		int tag = outline->tags[i];
		FT_Vector *curr = &outline->points[i];

		// label the control point
		buf[0] = i % 2 == 0 ? (char) ('a' + i / 2) : (char) ('A' + i / 2);
		printf("%d.%s/%d %ld,%ld %c\n", j, tag < 3 ? tags[tag] : "other", tag,
				curr->x, curr->y, buf[0]);
		rect_init(&rect, curr->x + offset_x, curr->y + offset_y,
				font->x_ppem_26_6 >> 6, font->ascender + font->descender);
		glyph_run_init(&grun, 0, 0);
		wchar_t *label = mbs2wcs(buf);
		text_draw(&bmp_canvas, label, font, &rect, &grun);
		free(label);

		if (contour_start == NULL)
			contour_start = curr;
		is_contour_end = i == outline->contours[j];

		if (tag & FT_CURVE_TAG_HAS_SCANMODE)
			printf("FT_CURVE_TAG_HAS_SCANMODE\n");

//		switch (tag) {
//		case FT_CURVE_TAG_ON:

		// http://www.freetype.org/freetype2/docs/glyphs/glyphs-6.html
		// Two successive ‘on’ points indicate a line segment joining them.

		// http://freetype.org/freetype2/docs/reference/ft2-outline_processing.html#FT_Outline
		if (tag & FT_CURVE_TAG_ON) {
			if (last_on) {
				printf(" |%s\n", last_conic ? "conic" : "line");
				if (last_conic)
					draw_conic(canvas, last_on, last_conic, curr);
				else
					draw_line(canvas, last_on, curr);
			}

			if (is_contour_end) {
				draw_line(canvas, curr, contour_start);
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
				temp.x = last_on->x;
				temp.y = last_on->y;
				virtual_on.x = (last_conic->x + curr->x) / 2;
				virtual_on.y = (last_conic->y + curr->y) / 2;
				draw_conic(canvas, &temp, last_conic, &virtual_on);
				printf(" |on/conic %ld %ld - %ld %ld\n", temp.x, temp.y,
						virtual_on.x, virtual_on.y);
				last_on = &virtual_on;
			}
			if (is_contour_end) {
				draw_conic(canvas, last_on, curr, contour_start);
				printf(" |on/conic %ld %ld - %ld %ld\n", last_on->x, last_on->y,
						contour_start->x, contour_start->y);
				last_on = NULL;
				last_conic = NULL;
			} else {
				last_conic = curr;
			}
		}

		if (is_contour_end) {
			j++;
			contour_start = NULL;
			continue;
		}
	}

	printf("#point=%d #contour=%d\n", outline->n_points, outline->n_contours);
}

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
			text_draw(&bmp_canvas, test, &font, &rect, &grun);

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

int x11_processor(XEvent *event) {
	int x, y;
	switch (event->type) {
	case MapNotify:
		printf("mapnot: %dx%d\n", bmp_canvas.rect.width,
				bmp_canvas.rect.height);

		window_image_create(&bmp_output, &img_output, 0, 0xff);
		if (window_rotation == 0)
			bmp_canvas = bmp_output;
		else
			window_image_create(&bmp_canvas, &img_canvas, 0, 0xff);

		if (0) {
			glyph_run_init(&grun, 0, 0);
			rect.left = 10;
			rect.top = 100;
			rect.height = bmp_canvas.rect.height - -rect.top * 2;
			rect.width = bmp_canvas.rect.width - rect.left * 2;
			text_draw(&bmp_canvas, test, &font, &rect, &grun);

			text_compare();
			FT_Face face = font.face;
			FTC_ScalerRec scaler = font.scaler;
			font.face = font.fallback;
			font.scaler = font.scaler_fallback;
			text_compare();
			font.face = face;
			font.scaler = scaler;
		} else {
			int fallback = 0;
//			fallback = 1;
			outline_draw(&bmp_canvas, test[0], &font, fallback);
//			outline_draw(&bmp_canvas, test[1], &font, 1);
		}
		window_repaint(NULL);
		break;
	case ResizeRequest:
		bmp_output.rect.width = event->xresizerequest.width;
		bmp_output.rect.height = event->xresizerequest.height;
		if (window_rotation == 90 || window_rotation == 270) {
			bmp_canvas.rect.width = event->xresizerequest.height;
			bmp_canvas.rect.height = event->xresizerequest.width;
		} else {
			bmp_canvas.rect.width = event->xresizerequest.width;
			bmp_canvas.rect.height = event->xresizerequest.height;
		}
		printf("resize: %dx%d\n", bmp_canvas.rect.width,
				bmp_canvas.rect.height);
		break;
	case ConfigureNotify:
		break;
	case Expose:
		break;
	case ButtonPress:
		x = event->xbutton.x;
		y = event->xbutton.y;
		if (window_rotation == 270) {
			int t = x;
			x = bmp_canvas.rect.width - y;
			y = t;
		}
//		printf("button %d: (%d, %d)\n", event->xbutton.button, x, y);
		break;
	case KeyPress:
		switch (event->xkey.keycode) {
		default:
			printf("x11key: 0x%x\n", event->xkey.keycode);
			break;
		}
		break;
	}

	return 1;
}

int main(int argc, char *argv[]) {
	setlocale(LC_CTYPE, "en_US.UTF-8");

	unsigned char filter[] = { 15, 60, 105, 60, 15 };
//	unsigned char filter[] = {0, 0, 255, 0, 0};
//	unsigned char filter[] = {28, 56, 85, 56, 28};
	int i;
	for (i = 0; i < 5; i++)
		subpixel_filter[i] = filter[i];

//	IPA fonts: CharisSIL-R.ttf DoulosSIL-R.ttf LinLibertine_R.otf
	freetype_init(2, 2, 10240 * 1024, 1);
//	freetype_font("fonts/XinGothic.otf", "fonts/方正兰亭细黑_GBK.ttf");
	freetype_font("/home/pxlogpx/.fonts/_H_HelveticaNeue.ttc", 4,
			"fonts/arial.ttf", 0);

	font_init(&font, font_height, 5);

	x11_init(1600, 1000);

	x11_set_title("Font Rendering");
	x11_show_window(0, x11_processor, NULL, NULL, 0);

	x11_free();
	freetype_free();

	return EXIT_SUCCESS;
}

//int x, y, t, s;
//if (p0->x == p1->x) {
//	if (p0->y == p1->y) {
//
//	}
//} else {
//	if (p0->x > p1->x) {
//		FT_Vector *p = p0;
//		p0 = p1;
//		p1 = p;
//	}
//
//	for (t = 0; t < 1; t += 0.01) {
//		s = 1 - t;
//		x = s * s * p0->x + 2 * s * t * p1->x + t * t * p2->x;
//		y = s * s * p0->y + 2 * s * t * p1->y + t * t * p2->y;
//	}
//}
