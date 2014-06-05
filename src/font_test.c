#include "wl_x11.h"
#include "wl_text.h"
#include <locale.h>
#include <stdint.h>

font_conf_t test_font;
wchar_t *test = L"T"; // Windows Steve 中文字体ɒɔ
glyph_run_t test_grun;
rect_s test_rect;
int font_height = 14;

void outline_draw(bitmap_t *canvas, wchar_t c, font_conf_t *font, char fallback) {
	FT_Glyph glyph;

	FT_Face ft_face = font->face;
	FTC_Scaler ft_scaler = &font->scaler;

	FT_UInt glyph_index = FT_Get_Char_Index(ft_face, (FT_ULong) c);
	if (fallback) {
		ft_face = font->fallback;
		ft_scaler = &font->scaler_fallback;
		glyph_index = FT_Get_Char_Index(ft_face, (FT_ULong) c);
	}
	if (glyph_index == 0)
		perror("glyph_index");
	if (FTC_ImageCache_LookupScaler(ft_cache, ft_scaler, font->load_flags,
			glyph_index, &glyph, NULL))
		perror("FTC_ImageCache_LookupScaler");

	FT_OutlineGlyph o = (FT_OutlineGlyph) glyph;
	FT_Outline *outline = &o->outline;

	char s[2];
	s[1] = 0;

	int i, j = 0;
//	for (i = 0; i < outline->n_contours; i++) {
//		short c = outline->contours[i];
//		printf("contour %d\n", c);
//	}

	for (i = 0; i < outline->n_points; i++) {
		FT_Vector p = outline->points[i];
		char tag = outline->tags[i];

		p.x += 150;
		p.y += 150;
		p.y = canvas->rect.height - p.y;

		s[0] = i % 2 == 0 ? (char) ('a' + i / 2) : (char) ('A' + i / 2);
		printf("%d: %ld/%ld %d %s %c %c\n", i, p.x, p.y, tag,
				tag == FT_CURVE_TAG_ON ?
						"ON" :
						(tag == FT_CURVE_TAG_CONIC ?
								"conic" :
								(tag == FT_CURVE_TAG_CUBIC ? "cubic" : "other")),
				s[0], i == outline->contours[j] ? '*' : ' ');
		if (i == outline->contours[j])
			j++;

		if (p.x < 1 || p.y < 1)
			continue;
		if (p.x + 1 >= canvas->rect.width || p.y + 1 >= canvas->rect.height)
			perror("over");
		rect_s rect = { p.x - 1, p.y - 1, 3, 3 };
//		rect_s rect = { 100, 100, 100, 100 };

//		bitmap_fill_rect(canvas, &rect, 0x00, 0x00, 0xff);
		rect.width = 20;
		rect.height = 20;
		test_grun.curr_pos = 0;
		test_grun.offset_y = 0;
		test_grun.offset_x_26_6 = 0;

		wchar_t *w = mbs2wcs(s);
		text_draw(&bmp_canvas, w, font, &rect, &test_grun);
		free(w);
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
			test_font.load_flags = flags[i];
			test_font.render_mode = modes[j];
			test_grun.curr_pos = 0;
			test_grun.offset_x_26_6 = 0;
			text_draw(&bmp_canvas, test, &test_font, &test_rect, &test_grun);

//			test_grun.curr_pos = 0;
//			test_grun.offset_x_26_6 = 0;
//			strcpy(s, model[j]);
//			strcat(s, "/");
//			strcat(s, flagl[i]);
//			wchar_t *w = mbs2wcs(s);
//			text_draw(&bmp_canvas, w, &test_font, &test_rect, &test_grun);
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

		glyph_run_init(&test_grun, 0, 0);
		test_rect.left = 10;
		test_rect.top = 100;
		test_rect.height = bmp_canvas.rect.height - -test_rect.top * 2;
		test_rect.width = bmp_canvas.rect.width - test_rect.left * 2;
		text_draw(&bmp_canvas, test, &test_font, &test_rect, &test_grun);

		text_compare();
		FT_Face face = test_font.face;
		FTC_ScalerRec scaler = test_font.scaler;
		test_font.face = test_font.fallback;
		test_font.scaler = test_font.scaler_fallback;
		text_compare();
		test_font.face = face;
		test_font.scaler = scaler;

//		int fallback = 0;
//		fallback = 1;
//		outline_draw(&bmp_canvas, test[0], &test_font, fallback);
//		outline_draw(&bmp_canvas, test[1], &test_font, 1);
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

	unsigned char filter[] = {15, 60, 105, 60, 15};
//	unsigned char filter[] = {0, 0, 255, 0, 0};
//	unsigned char filter[] = {28, 56, 85, 56, 28};
	int i;
	for (i = 0; i < 5; i++)
		subpixel_filter[i] = filter[i];

//	IPA fonts: CharisSIL-R.ttf DoulosSIL-R.ttf LinLibertine_R.otf
	freetype_init(2, 2, 10240 * 1024, 1);
//	freetype_font("fonts/XinGothic.otf", "fonts/方正兰亭细黑_GBK.ttf");
	freetype_font("/home/pxlogpx/.fonts/_H_HelveticaNeue.ttc", 4, "fonts/arial.ttf",
			0);

	font_init(&test_font, font_height, 5);

	x11_init(1000, 800);

	x11_set_title("Font Rendering");
	x11_show_window(0, x11_processor, NULL, NULL, 0);

	x11_free();
	freetype_free();

	return EXIT_SUCCESS;
}
