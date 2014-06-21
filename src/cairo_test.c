#include "wl_x11.h"
#include <pango/pangocairo.h>
#include <cairo.h>
#include <cairo-xlib.h>

int USE_XLIB_SURFACE = 0;
const int font_size = 16;
const char *font_name = "Menlo"; // "Heiti SC";
//const char *font_name = ".Helvetica NeueUI";

//const char *TEXT =
//		"到底中国的手游CP们是否该酝酿着掘金WP呢？我们先听听微软大佬的‘辩词’。\n"
//				"　　首先，WP平台的竞争优势可以总结为以下几点：\n"
//				"　　1）WP手机的用户质量比安卓好太多了。。。交流会上大家有这样一个共识：用Lumia 1520的全是高富帅，花钱下app特别肆意的内种。事实上，从每用户收入、用户粘性、付费转化率等多个维度的数据来看，WP平台的用户质量跟iOS差不了太多。小编这里就晒一组数据：（根据Statistics Brain）WP应用的单次下载收入平均是23美分，iOS是24美分，而安卓则是可怜的4美分。\n"
//				"　　2）对于开发者来说，做一款WP游戏的门槛（启动成本）要比安卓和iOS低出很多。如今的iOS和安卓平台都就有点饱和的意思，让一款游戏脱颖而出何其之难，开发者的竞争压力何其之大；而WP的市场份额虽然依旧单薄，但是增长率健康。交流会上有几个国内游戏厂商直言不讳地表示：在iOS上推一款游戏，扔1000万，撑死能烧7天；WP则不一样，200万的投入能让你烧14天到一个月。\n"
//				"\n"
//				"\"It was a particularly heinous case,\" Miller told the Monterey Herald. The children had \"hardly eaten for months.\"\n"
//				"\n"
//				"The boys are 3 and 5 years old, and the girl is 8, authorities said, and they all exhibited bruises and signs of other physical as well as emotional abuse.\n"
//				"\n";
const char *TEXT =
		"/Sample: The quick brown fox jumps over the lazy dog. 1234567890";

void pango_draw_text(cairo_t *cr, int x, int y, int w, int h) {
	cairo_move_to(cr, x, y);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_layout_set_width(layout, w * PANGO_SCALE);
	pango_layout_set_height(layout, h * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_text(layout, TEXT, -1);
	PangoFontDescription *desc = pango_font_description_from_string(font_name);
	pango_font_description_set_size(desc, font_size * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);

	pango_font_description_free(desc);

	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
}

void cairo_draw_text(cairo_t *cr, int x, int y) {
	cairo_move_to(cr, x, y);

	cairo_select_font_face(cr, font_name, CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);

	cairo_font_options_t *font_options = cairo_font_options_create();
	cairo_get_font_options(cr, font_options);
	cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(font_options,
			CAIRO_SUBPIXEL_ORDER_RGB);
	cairo_set_font_options(cr, font_options);
	cairo_font_options_destroy(font_options);

	cairo_show_text(cr, TEXT);
}

void render() {
	// RGB24 unsupported?
	cairo_surface_t *surface = NULL;
	if (USE_XLIB_SURFACE) {
		surface = cairo_xlib_surface_create(display, window, visual,
				img_output->width, img_output->height);
		printf("surface created\n");
	} else {
		surface = cairo_image_surface_create_for_data(bmp_canvas.data,
				CAIRO_FORMAT_ARGB32, bmp_canvas.rect.width,
				bmp_canvas.rect.height, bmp_canvas.bytes_per_line);
	}

	cairo_t *cr = cairo_create(surface);
	cairo_get_scaled_font(cr);
//	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // red
//	cairo_paint(cr);

	if (0) {
		cairo_set_source_rgb(cr, 1.0, 0, 0); // red
//		cairo_set_line_width(cr, 2);
//		cairo_move_to(cr, 100, 100);
//		cairo_line_to(cr, 200, 200);
		cairo_rectangle(cr, 10, 10, 100, 100);
		cairo_fill(cr);
	} else {
//		pango_draw_text(cr, 10, 10, img_output->width - 20,
//				img_output->height - 20);
		cairo_draw_text(cr, 10, 30);

//		cairo_glyph_t *glyphs;
//		int num_glyphs;
//		cairo_show_glyphs(cr, glyphs, num_glyphs);
		cairo_text_extents_t extents;
		cairo_text_extents(cr, TEXT, &extents);
	}

	cairo_destroy(cr);

//	cairo_surface_finish(surface);
	cairo_surface_destroy(surface);
}

int x11_processor(XEvent *event) {
	switch (event->type) {
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
	case MapNotify:
		printf("mapnot: %dx%d\n", bmp_canvas.rect.width,
				bmp_canvas.rect.height);
		if (!USE_XLIB_SURFACE) {
			window_image_create(&bmp_output, &img_output, 1, 0xff);
			if (window_rotation == 0)
				bmp_canvas = bmp_output;
			else
				window_image_create(&bmp_canvas, &img_canvas, 1, 0xff);
		}

		render();

		if (!USE_XLIB_SURFACE) {
			rect_s rect;
			rect_init(&rect, 100, 100, 200, 200);
			bitmap_draw_rect(&bmp_canvas, &rect, 0x00);
			window_repaint(NULL);
		}

		break;
	case ConfigureNotify:
		break;
	case Expose:
		break;
	case KeyPress:
		switch (event->xkey.keycode) {
		}
		break;
	}
	return 1;
}

int main(int argc, char *argv[]) {
	x11_init(800, 600);
	x11_set_title("hack_cairo");
	x11_show_window(0, x11_processor, NULL, NULL, 0);
	x11_free();
	return 0;
}
