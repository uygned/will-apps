#include "wl_mupdf.h"
#include "wl_touch.h"
#include "porter_stemmer.h"
#include <locale.h>
#include <dirent.h>

//#define KINDLE
#define TOUCH_MOVEMENT_MIN 20

#ifdef KINDLE
#include "wl_einkfb.h"
#endif

#ifndef NO_X11
#include "will_pdf_x11.h"
#else
#include "will_pdf_eink.h"
#endif

#define TOOLBAR_HEIGHT 100

void app_quit() {
#ifndef NO_X11
	x11_quit = 1;
#endif
	touch_reader_quit = 1;
}

int file_type_is(const char *file, const char *type) {
	if (!file || !type)
		return 0;

	int i = strlen(file);
	int j = strlen(type);
	while (i > 0 && j > 0) {
		if (file[--i] != type[--j])
			return 0;
	}
	return j == 0;
}

void filebox_show() {
	grid_view_free(&filebox);
	grid_view_init(&filebox, NULL, 20, 1, &filebox_font);
	if (filebox.rect.width == 0) {
		filebox.rect = (rect_s ) { 10, TOOLBAR_HEIGHT, bmp_canvas.rect.width
						- 20, bmp_canvas.rect.height - TOOLBAR_HEIGHT * 2 };
	}

	DIR *d = opendir(filebox_path);
	if (d == NULL) {
		printf("error: opendir() %s\n", filebox_path);
		return;
	}

	struct dirent *e;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0
				|| (e->d_name[0] == '.' && strcmp(e->d_name, "..") != 0))
			continue;
		if (e->d_type == DT_DIR) {
			char name[PATH_MAX];
//			name[0] = 0;
//			strcat(name, e->d_name);
			strcpy(name, e->d_name);
			strcat(name, "/");
			grid_item_add(&filebox, name, NULL, NULL);
		} else if (e->d_type == DT_REG) {
			if (file_type_is(e->d_name, ".pdf"))
				grid_item_add(&filebox, e->d_name, NULL, NULL);
		}
	}
	closedir(d);
	grid_view_draw(&bmp_canvas, &filebox);
	window_repaint(&filebox.rect);
}

int filebox_clicked(int i, int x, int y) {
	char *file = wcs2mbs(filebox.items[i]->title);
	if (file_type_is(file, ".pdf")) {
		// open pdf file
		strcat(filebox_path, "/");
		strcat(filebox_path, file);
		pdf_load(filebox_path, 0, 0);
		strrchr(filebox_path, '/')[0] = 0;

		filebox.visible = 0;
		pdf_paint(&bmp_canvas);
		window_repaint(NULL);
	} else if (strcmp(file, "../") == 0) {
		// enter parent directory
		strrchr(filebox_path, '/')[0] = 0;
		if (strlen(filebox_path) == 0)
			strcpy(filebox_path, "/");
		filebox_show();
	} else {
		// enter child directory
		file[strlen(file) - 1] = 0; // remove the ending '/'
		strcat(filebox_path, "/");
		strcat(filebox_path, file);
		filebox_show();
	}
	free(file);
	return 1;
}

void numpad_show() {
	if (numpad.rect.width == 0) {
		int width = 450;
		int height = 300;
		numpad.padding_top = 50;
		numpad.item_padding_left = width / 10;
		rect_init(&numpad.rect, (bmp_canvas.rect.width - width) / 2,
				(bmp_canvas.rect.height - height) / 2, width, height);
	}
	numpad_len = 0;
	numpad_buf[0] = 0;
	grid_view_draw(&bmp_canvas, &numpad);
	window_repaint(&numpad.rect);
	glyph_run_init(&numpad_run, 0, 0);
}

int numpad_clicked(int i, int x, int y) {
	if (strcmp(numpad_keys[i], "Submit") == 0) {
		if (numpad_len > 0) {
			numpad_buf[numpad_len] = 0;
			int n = atoi(numpad_buf);
//			printf("numpad: %d\n", n);
			if (n <= pdf_page_count) {
				pdf_run.page_top = 0;
				if (numpad_buf[0] == '-' || numpad_buf[0] == '+')
					pdf_run.page_no += n;
				else
					pdf_run.page_no = n;
			}
		}
		numpad.visible = 0;
		pdf_paint(&bmp_canvas);
		window_repaint(NULL);
	} else if (strcmp(numpad_keys[i], "Clear") == 0) {
		numpad_show();
	} else {
		// number key clicked, draw clicked digit char
		numpad_buf[numpad_len++] = numpad_keys[i][0];
		numpad_run.curr_pos = 0;
		numpad_run.offset_y = 0;
		rect_s rect;
		rect_init(&rect,
				numpad.rect.left + numpad.padding_left
						+ numpad.item_padding_left, numpad.rect.top + 10,
				numpad.rect.width - numpad.item_padding_left,
				default_font.ascender + default_font.descender);
		text_draw(&bmp_canvas, numpad.items[i]->title, &default_font, &rect,
				&numpad_run);
		window_repaint(&rect);
	}

	return 1;
}

void dict_show(int x, int y) {
	hashmap_entry_t *e = NULL;
	int len = pdf_get_text(&bmp_canvas, x, y, dict_word, dict_word_len);
//	char *test = "stopping";
//	strcpy(dict_word, test);
//	len = strlen(test);
	if (len > 0) {
		int i = len;
		while (--i >= 0) {
			char c = dict_word[i];
			if ('A' <= c && c <= 'Z')
				dict_word[i] = c + 0x20; // to lowercase
		}

		e = stardict_find(dict_word);
		if (e == NULL) {
			int k = stem(dict_word, 0, len - 1); // stemming
			if (k >= 0) { // Todo stop words
				char c = dict_word[k + 1];
				dict_word[k + 1] = 0;
				printf("%s %d\n", dict_word, k);
				e = stardict_find(dict_word);
				if (e && strcmp(dict_word, e->key) != 0) {
					printf("[DICT] find error\n");
					e = NULL;
				}
				dict_word[k + 1] = c;
				if (e == NULL && len > 5) {
					dict_word[k + 1] = c;
					// stemming: creased -> creas
					if (k == len - 3 && dict_word[len - 2] == 'e'
							&& dict_word[len - 1] == 'd') {
						k++;
						c = dict_word[k + 1];
						dict_word[k + 1] = 0;
						e = stardict_find(dict_word);
					}
				}
			}
		} else if (strcmp(dict_word, e->key) != 0) {
			printf("[DICT] find error\n");
			e = NULL;
		}

		if (e) {
//			strcpy(dict_word, e->key);
//			printf("[DICT] %s/%s: %ls\n", dict_word, e->key, (wchar_t *) e->data);
			strcat(dict_word, " ~ ");
		}
	} else {
		strcpy(dict_word, "[N/A]");
	}

	rect_s dict_rect;
	glyph_run_t dict_run;
	rect_init(&dict_rect, 10/*left margin*/, min(5, PDF_PADDING_TOP),
			bmp_canvas.rect.width - 20/*and right margin*/,
			default_font.ascender + default_font.descender + 5);
	glyph_run_init(&dict_run, 0, 0);

	bitmap_fill_rect(&bmp_canvas, &dict_rect, 0xff, 0xff, 0xff);

	wchar_t *word = mbs2wcs(dict_word);
	text_draw(&bmp_canvas, word, &default_font, &dict_rect, &dict_run);
	free(word);

	if (e) {
		dict_run.offset_y = 0;
		dict_run.curr_pos = 0;
		text_draw(&bmp_canvas, (wchar_t *) e->data, &default_font, &dict_rect,
				&dict_run);
	}

	window_repaint(&dict_rect);
}

void touch_processor(touch_data_t *touch) {
	if (touch->paths[1][0].x == -1) { // single-finger tap
		// TODO edge snapping
		int x1 = touch->paths[0][0].x;
		int y1 = touch->paths[0][0].y;
		int x2 = touch->paths[0][TOUCH_POINT_MAX].x;
		int y2 = touch->paths[0][TOUCH_POINT_MAX].y;
		int x_delta = x2 == -1 ? 0 : x2 - x1;
		int y_delta = y2 == -1 ? 0 : y2 - y1;
		x1 += x_delta / 2;
		y1 += y_delta / 2;
		if (window_rotation == 270) {
			int t = x1;
			x1 = bmp_canvas.rect.width - y1;
			y1 = t;
		}

		// dictionary
		if (dict_mode) {
			dict_show(x1, y1);
			dict_mode = 0;
			return;
		}

		// touch movement
		if (abs(x_delta) > TOUCH_MOVEMENT_MIN
				|| abs(y_delta) > TOUCH_MOVEMENT_MIN) {
			if (window_rotation == 270) {
				int t = x_delta;
				x_delta = -y_delta;
				y_delta = t;
			}
//			printf("[TOUCH] scroll %d/%d\n", x_delta, y_delta);
			pdf_scroll(&bmp_canvas, -x_delta / 2, -y_delta / 2);
			window_repaint(NULL);
			return;
		}

//		printf("[TOUCH] point=%d/%d delta=%d/%d\n", x1, y1, delta_x,
//				delta_y);

		// widgets opened by virtual toobar
		if (grid_view_clicked(&numpad, x1, y1) == 1)
			return;
		if (grid_view_clicked(&filebox, x1, y1) == 1)
			return;

		// virtual toolbar
		if (y1 < TOOLBAR_HEIGHT) {
			int w = bmp_canvas.rect.width / 3;
			if (x1 < w) {
				app_quit(); // left part
			} else if (x1 < bmp_canvas.rect.width - w) {
				numpad_show(); // center part
			} else {
				filebox_show(); // right part
			}
			return;
		} else if (y1 > bmp_canvas.rect.height - TOOLBAR_HEIGHT) {
			int w = bmp_canvas.rect.width / 3;
			if (x1 < w) {
				dict_mode = 1;
			}
			return;
		}

		// pdf page up/down
		int s = bmp_canvas.rect.height - PDF_PADDING_TOP - pdf_line_height;
		pdf_scroll(&bmp_canvas, 0, x1 < bmp_canvas.rect.width / 2 ? -s : s);
		window_repaint(NULL);
	} else { // double-finger tap
		point_t *beg1 = &touch->paths[0][0];
		point_t *beg2 = &touch->paths[1][0];
		point_t *end1 = &touch->paths[0][TOUCH_POINT_MAX];
		point_t *end2 = &touch->paths[1][TOUCH_POINT_MAX];
		// TODO if points > 2
		if (end1->x == -1)
			end1 = &touch->paths[0][0];
		if (end2->x == -1)
			end2 = &touch->paths[1][0];

		int begin_x = (beg1->x + beg2->x) / 2;
		int begin_y = (beg1->y + beg2->y) / 2;
		double times = distance_of(end1, end2) / distance_of(beg1, beg2);
		int scale = 100 * sqrt(times);
//		printf("scale: %f->%d (%d,%d)\n", times, scale, begin_x, begin_y);
		pdf_scale(&bmp_canvas, scale, scale, begin_x, begin_y);
		window_repaint(NULL);
	}
}

int main(int argc, char *argv[]) {
	setlocale(LC_CTYPE, "en_US.UTF-8");

	freetype_init(2, 2, 10240 * 1024, 0);

	// IPA fonts: CharisSIL-R.ttf DoulosSIL-R.ttf LinLibertine_R.otf
	// http://www.linguiste.org/chart.html
	char *font_main = getenv("MAIN_FONT_PATH");
	if (font_main == NULL)
		font_main = "fonts/XinGothic.otf";
	char *font_fallback = getenv("FALLBACK_FONT_PATH");
	if (font_fallback == NULL)
		font_fallback = "fonts/arial.ttf";
	freetype_font(font_main, 0, font_fallback, 0);

	int font_height = 25;
	font_init(&filebox_font, font_height, 5);
	font_init(&default_font, font_height, 10);

//	char *cmds[] = { "[Open]", "[PgUp]", "[PgDn]", "[GoTo]", "[Quit]" };
//	grid_view_init(&toolbar, cmds, 5, 5, FONT_HEIGHT, LINE_SPACING);

	grid_view_init(&numpad, numpad_keys, 14, 3, &default_font);
	grid_view_init(&filebox, NULL, 0, 0, &filebox_font);
	numpad.clicked = numpad_clicked;
	filebox.clicked = filebox_clicked;

#ifdef KINDLE
	window_rotation = 270;
	char *home = "/mnt/us/documents";
#else
//	window_rotation = 270;
	char *home = getenv("HOME");
#endif
	strcpy(filebox_path, home);

	stardict_init("dicts");

//	char *pdf_file = argc >= 2 ? argv[1] : "test/t3.pdf";
//	pdf_load(pdf_file, 0, 0);
	if (argc >= 2) {
		pdf_load(argv[1], 0, 0);
		if (argc >= 3)
			pdf_run.page_no = atoi(argv[2]);
	}

#ifndef NO_X11

	x11_init(1024, 758);

#ifdef KINDLE
	char *home = "/mnt/us/documents"
	strcpy(filebox_path, home, strlen(home));

	einkfb_init();
	touch_reader_init(touch_processor, 0);

	int fds[] = {touch_input_fd};
	fd_read_callback_t cbs[] = {touch_reader_read};
	x11_set_title("L:A_N:application_ID:WillPDF");
	x11_show_window(StructureNotifyMask | ResizeRedirectMask, x11_processor,
			fds, cbs, 1);

	einkfb_init();
	touch_reader_free();
#else
	x11_set_title("WillPDF");
	x11_show_window(0, x11_processor, NULL, NULL, 0);
#endif /*KINDLE*/

	x11_free();

#else /*NO_X11*/
	touch_reader_init(touch_processor, 0);
	einkfb_init();
	bitmap_init(window_rotation);

	pdf_paint(&bmp_canvas);
	window_repaint(NULL);

	touch_reader_run(0);

	bitmap_free();
	einkfb_free();
	touch_reader_free();
#endif

	pdf_free();
	stardict_free();
	freetype_free();
	grid_view_free(&numpad);
	grid_view_free(&filebox);

	return EXIT_SUCCESS;
}
