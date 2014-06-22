#include "wl_mupdf.h"

#ifdef MUPDF_TRACE
static int memtrace_current = 0;
static int memtrace_peak = 0;
static int memtrace_total = 0;

static void *trace_malloc(void *arg, unsigned int size) {
	if (size == 0)
	return NULL;
	int *p = malloc(size + sizeof(unsigned int));
	if (p == NULL)
	return NULL;
	p[0] = size;
	memtrace_current += size;
	memtrace_total += size;
	if (memtrace_current > memtrace_peak) {
		memtrace_peak = memtrace_current;
		if (memtrace_peak > 52428800)
		printf("[MUPDF] memtrace_peak=%d\n", memtrace_peak);
	}
	return (void *) &p[1];
}

static void trace_free(void *arg, void *p_) {
	int *p = (int *) p_;
	if (p == NULL)
	return;
	memtrace_current -= p[-1];
	free(&p[-1]);
}

static void *trace_realloc(void *arg, void *p_, unsigned int size) {
	int *p = (int *) p_;
	unsigned int oldsize;

	if (size == 0) {
		trace_free(arg, p_);
		return NULL;
	}
	if (p == NULL)
	return trace_malloc(arg, size);
	oldsize = p[-1];
	p = realloc(&p[-1], size + sizeof(unsigned int));
	if (p == NULL)
	return NULL;
	memtrace_current += size - oldsize;
	if (size > oldsize)
	memtrace_total += size - oldsize;
	if (memtrace_current > memtrace_peak) {
		memtrace_peak = memtrace_current;
		if (memtrace_peak > 52428800)
		printf("[MUPDF] memtrace_peak=%d\n", memtrace_peak);
	}
	p[0] = size;
	return &p[1];
}

fz_alloc_context alloc_ctx = {NULL, trace_malloc, trace_realloc, trace_free};
#endif

pdf_run_t pdf_run;
//rect_s pdf_view;
int pdf_page_count = 0;
unsigned char pdf_background = 0xff;
ushort pdf_line_height = 50;

static fz_context *pdf_ctx = NULL;
static fz_document *pdf_doc = NULL;
static fz_text_sheet *pdf_sheet = NULL;
static pdf_page_t *pdf_pages[PDF_PAGE_CACHE_SIZE];

void pdf_run_init(pdf_run_t *prun, int page_no, int scale, int rotation) {
	prun->page_no = page_no;
	prun->page_left[0] = 0;
	prun->page_left[1] = 0;
	prun->page_top = 0;
	prun->scale_x = scale;
	prun->scale_y = scale;
	prun->rotation = rotation;
}

static void pdf_page_free(pdf_page_t *page) {
	if (page) {
		if (page->pixmap) {
			//printf("[DEBUG] fz_drop_pixmap %d\n", page->pixmap->storable.refs);
			fz_drop_pixmap(pdf_ctx, page->pixmap);
			page->pixmap = NULL;
		}
		if (page->page) {
			fz_free_page(pdf_doc, page->page);
			page->page = NULL;
		}
	}
}

static void pdf_page_reset(pdf_page_t *page) {
	pdf_page_free(page);
	pdf_run_init(&page->run, 0, 0, 0);
	page->pixmap_width = 0;
	page->pixmap_height = 0;
}

void pdf_free() {
	int i;
	for (i = 0; i < PDF_PAGE_CACHE_SIZE; i++) {
		if (pdf_pages[i]) {
			pdf_page_free(pdf_pages[i]);
			free(pdf_pages[i]);
		}
	}
	if (pdf_doc) {
		fz_close_document(pdf_doc);
		pdf_doc = NULL;
	}
	if (pdf_ctx) {
		fz_free_context(pdf_ctx);
		pdf_ctx = NULL;
	}
}

int pdf_load(const char *file, int scale, int rotation) {
	if (pdf_ctx == NULL) {
		// the first load call, do initialization
		pdf_ctx = fz_new_context(NULL, NULL, 50 * 1024 * 1024); // FZ_STORE_DEFAULT
		if (pdf_ctx == NULL) {
			printf("error: fz_new_context()\n");
			return 0;
		}
		fz_register_document_handlers(pdf_ctx);
		int i;
		for (i = 0; i < PDF_PAGE_CACHE_SIZE; i++) {
			pdf_pages[i] = malloc(sizeof(pdf_page_t));
			pdf_pages[i]->page = NULL;
			pdf_pages[i]->pixmap = NULL;
			pdf_page_reset(pdf_pages[i]);
		}
	} else {
		int i;
		for (i = 0; i < PDF_PAGE_CACHE_SIZE; i++) {
			pdf_page_reset(pdf_pages[i]); // no malloc() again
		}
		if (pdf_doc) {
			fz_close_document(pdf_doc);
			pdf_doc = NULL;
		}
		if (pdf_sheet) {
			fz_free_text_sheet(pdf_ctx, pdf_sheet);
			pdf_sheet = NULL;
		}
	}

	pdf_run_init(&pdf_run, 1, scale, rotation);

//	fz_try (pdf_ctx) {
//	} fz_catch (pdf_ctx) {
//	}

	pdf_doc = fz_open_document(pdf_ctx, file);
	pdf_page_count = fz_count_pages(pdf_doc);
	if (!pdf_doc) {
		printf("error: fz_open_document()\n");
		return 0;
	} else {
		printf("[MUPDF] #page=%d\n", pdf_page_count);
		return 1;
	}
}

static int pdf_get_page(bitmap_t *canvas, pdf_page_t *page, int page_no,
		int scale_x, int scale_y, int rotation) {
	if (!canvas || !page)
		return 0;

//	printf("[MUPDF] page=%d scale=%d,%d rotate=%d\n", page_no, scale_x, scale_y,
//			rotation);

	if (page->pixmap && (page->run.page_no == page_no)
			&& (page->run.scale_x == scale_x && page->run.scale_y == scale_y)
			&& (page->run.rotation == rotation))
		return 1;

	if (page->run.page_no != page_no) {
		if (page->page)
			fz_free_page(pdf_doc, page->page);
		page->page = fz_load_page(pdf_doc, page_no - 1);
		fz_bound_page(pdf_doc, page->page, &page->rect);
	}

	if (scale_x == 0) {
		if (scale_y != 0) {
			scale_x = scale_y;
		} else if (page->rect.x1 - page->rect.x0 != 0) {
			scale_x = 100.0 * canvas->rect.width
					/ (page->rect.x1 - page->rect.x0);
			printf("[MUPDF] auto_scale=%d(%dx%d/%.0fx%.0f) (%.0f,%.0f)\n",
					scale_x, canvas->rect.width, canvas->rect.height,
					page->rect.x1, page->rect.y1, page->rect.x0, page->rect.y0);
		} else {
			scale_x = 100;
		}
	}

	if (scale_y == 0)
		scale_y = scale_x;

	if (page->pixmap)
		fz_drop_pixmap(pdf_ctx, page->pixmap);

	fz_matrix mat; // fz_identity
	fz_rotate(&mat, rotation);
	fz_pre_scale(&mat, scale_x / 100.0, scale_y / 100.0);

	fz_rect rect = page->rect;
	fz_transform_rect(&rect, &mat); // do transform on rect copy

	fz_irect bbox;
	fz_round_rect(&bbox, &rect);

	fz_colorspace *cs =
			canvas->bytes_per_pixel == 1 ?
					fz_device_gray(pdf_ctx) : fz_device_rgb(pdf_ctx);
	page->pixmap = fz_new_pixmap_with_bbox(pdf_ctx, cs, &bbox);

	fz_keep_pixmap(pdf_ctx, page->pixmap);
	fz_drop_pixmap(pdf_ctx, page->pixmap);
	fz_clear_pixmap_with_value(pdf_ctx, page->pixmap, pdf_background);
//	printf("[DEBUG] fz_keep_pixmap %d\n", page->pixmap->storable.refs);

	fz_device *dev = fz_new_draw_device(pdf_ctx, page->pixmap);
	fz_run_page(pdf_doc, page->page, dev, &mat, NULL);
	fz_free_device(dev);

	page->run.page_no = page_no;
	page->run.scale_x = scale_x;
	page->run.scale_y = scale_y;
	page->run.rotation = rotation;
	// page_top page_left are updated in pdf_page_paint()
	page->pixmap_width = page->pixmap->w;
	page->pixmap_height = page->pixmap->h;

//	printf("[MUPDF] scale=%d,%d rotate=%d bounds=(%.0f,%.0f)(%.0f,%.0f)\n",
//			scale_x, scale_y, rotation, rect.x0, rect.y0, rect.x1, rect.y1);

//	printf("[MUPDF] pixmap_bbox=%d,%d/%d,%d colorspace=%d\n", bbox.x0, bbox.y0,
//			bbox.x1, bbox.y1, cs->n);

//	fz_write_png(pdf_ctx, page->pixmap, "out.png", 0);

	return 1;
}

// search line who's pixels are color between [y_start, y_end)
static int pixmap_search_line(fz_pixmap *pixmap, int y_start, int y_end,
		unsigned char color) {
	int src_w = pixmap->w;
	int src_h = pixmap->h;
	if (y_start >= src_h)
		return -1;
	int src_pixel_size = pixmap->n;
	if (src_pixel_size != 2 && src_pixel_size != 4)
		perror("bad pixel size (mupdf pixmap)");
	int src_line_size = src_w * src_pixel_size;
	unsigned char *src = fz_pixmap_samples(pdf_ctx, pixmap);
	src += y_start * src_line_size;

	int i, j;
	if (y_end > y_start) {
		for (j = y_start; j < src_h && j < y_end; j++) {
			char match = 1;
			unsigned char *s = src;
			for (i = 0; i < src_w; i++) {
				if (s[0] != color
						|| (src_pixel_size == 4
								&& (s[1] != color || s[2] != color))) {
					match = 0;
					break;
				}
				s += src_pixel_size;
			}
			if (match)
				return j;
			src += src_line_size;
		}
	} else {
		for (j = y_start; j >= 0 && j > y_end; j--) {
			char match = 1;
			unsigned char *s = src;
			for (i = 0; i < src_w; i++) {
				if (s[0] != color
						|| (src_pixel_size == 4
								&& (s[1] != color || s[2] != color))) {
					match = 0;
					break;
				}
				s += src_pixel_size;
			}
			if (match)
				return j;
			src -= src_line_size;
		}
	}
	return -1;
}

// dst_x, dst_y are given as relative to canvas (left, top)
static int pdf_paint_pixmap(bitmap_t *canvas, pdf_page_t *page, int src_x,
		int src_y, int dst_x, int dst_y) {
	fz_pixmap *pixmap = page->pixmap;
	int src_w = pixmap->w;
	int src_h = pixmap->h;
	int src_pixel_size = pixmap->n;
	if (src_pixel_size != 2 && src_pixel_size != 4)
		perror("bad pixel size (mupdf pixmap)");
	int src_line_size = src_w * src_pixel_size;
	unsigned char *src = fz_pixmap_samples(pdf_ctx, pixmap);
	src += src_y * src_line_size + src_x * src_pixel_size;

	int dst_pixel_size = canvas->bytes_per_pixel;
	if (dst_pixel_size != 1 && dst_pixel_size != 3)
		perror("bad pixel size (mupdf canvas)");
	int dst_line_size = canvas->bytes_per_line;
	unsigned char *dst = canvas->data + canvas->rect.top * dst_line_size
			+ canvas->rect.left * dst_pixel_size;
	dst += dst_y * dst_line_size + dst_x * dst_pixel_size;

	int w = canvas->rect.width - dst_x;
	int h = canvas->rect.height - dst_y;
	int src_x2 = min(src_w, src_x + w);
	int src_y2 = min(src_h, src_y + h);
	int i, j;
	for (j = src_y; j < src_y2; j++) {
		unsigned char *s = src;
		unsigned char *d = dst;
		for (i = src_x; i < src_x2; i++) {
			*d++ = *s++;
			if (dst_pixel_size == 3) {
				*d++ = *s++;
				*d++ = *s++;
			}
			s++; // the alpha channel
		}
		src += src_line_size;
		dst += dst_line_size;
	}

	page->run.page_left[page->run.page_no % 2] = src_x;
	page->run.page_top = src_y;
//	printf("page paint: %d %d,%d\n", page->run.page_no, src_x, src_y);

	return j - src_y;
}

int pdf_paint(bitmap_t *canvas) {
	if (!canvas || !canvas->data || !pdf_doc)
		return 0;

	pdf_page_t *page = pdf_pages[0];
//	printf("[MUPDF] #%d scale=%d/%d pos=%d/%d #%d(%d/%d %d/%d) %d/%d\n",
//			pdf_state.page_no, pdf_state.scale_x, pdf_state.scale_y,
//			pdf_state.page_left, pdf_state.page_top, page->state.page_no,
//			page->state.scale_x, page->state.scale_y, page->state.page_left,
//			page->state.page_top, pdf_state.rotation, page->state.rotation);

	// reuse the next page
	if (page->run.page_no != pdf_run.page_no) {
		pdf_pages[0] = pdf_pages[1];
		pdf_pages[1] = page;
		page = pdf_pages[0];
	}

	pdf_get_page(canvas, page, pdf_run.page_no, pdf_run.scale_x,
			pdf_run.scale_y, pdf_run.rotation);

	// update auto determined scales
	if (pdf_run.scale_x != page->run.scale_x)
		pdf_run.scale_x = page->run.scale_x;
	if (pdf_run.scale_y != page->run.scale_y)
		pdf_run.scale_y = page->run.scale_y;

	int page_left = pdf_run.page_left[pdf_run.page_no % 2];
	//         |<- canvas width ->|
	// case 1: |<- page width ->|
	// case 2: |<- page width     ->|
	if (page_left < 0 || page->pixmap_width <= canvas->rect.width)
		page_left = 0;
	else if (page_left > page->pixmap_width - canvas->rect.width)
		page_left = page->pixmap_width - canvas->rect.width;
	pdf_run.page_left[pdf_run.page_no % 2] = page_left;

	if (pdf_run.page_top < 0) {
		pdf_run.page_top = 0;
	} else if (pdf_run.page_top > page->pixmap_height
			|| pdf_run.page_no == pdf_page_count) {
		if (page->pixmap_height <= canvas->rect.height)
			pdf_run.page_top = 0;
		else if (pdf_run.page_top > page->pixmap_height - canvas->rect.height)
			pdf_run.page_top = page->pixmap_height - canvas->rect.height;
	}

//	printf("H=%d %d page_top=%d\n", canvas->rect.height, page->run.page_no,
//			pdf_run.page_top);

	if (pdf_run.page_top > 0) {
		int blank = pixmap_search_line(page->pixmap, pdf_run.page_top,
				pdf_run.page_top - pdf_line_height, pdf_background);
		if (blank >= 0 && blank != pdf_run.page_top) {
			pdf_run.page_top = blank;
//			printf("  page_top=%d\n", blank);
		}
	}

	int page_y = pdf_run.page_top;
	int canvas_y = PDF_PADDING_TOP; // relative to (canvas.left, canvas.top)
	int painted_height = pdf_paint_pixmap(canvas, page, page_left, page_y,
			0/*pdf_view.left*/, canvas_y/*pdf_view.top*/);
	canvas_y += painted_height;

	if (pdf_run.page_no < pdf_page_count
//			&& page->pixmap_height - pdf_run.page_top < canvas->rect.height) {
			&& canvas_y + PDF_PAGE_MIN_HEIGHT < canvas->rect.height) {
//		int canvas_y = page->pixmap_height - pdf_run.page_top;

		// draw page seperator
		bitmap_dash_line(canvas, 0, canvas_y++, canvas->rect.width);

		// draw next page
		page = pdf_pages[1];
		pdf_get_page(canvas, page, pdf_run.page_no + 1, pdf_run.scale_x,
				pdf_run.scale_y, pdf_run.rotation);

		page_y = 0;
		painted_height = pdf_paint_pixmap(canvas, page, page_left, page_y, 0,
				canvas_y);
		canvas_y += painted_height;
	}

	int bottom = page_y + painted_height;
//	printf("h=%d %d page_bottom=%d\n", canvas->rect.height, page->run.page_no,
//			bottom);

	if (bottom < page->pixmap->h) { // the page is not fully painted
		int blank = pixmap_search_line(page->pixmap, bottom,
				bottom - pdf_line_height, pdf_background);
		if (blank >= 0 && blank != bottom) {
//			printf("search %d -> %d\n", bottom, blank);
			rect_s rect = canvas->rect; // copy
			rect.left = 0;
			rect.top = canvas_y - (bottom - blank);
			rect.height -= rect.top;
			bitmap_fill_rect(canvas, &rect, pdf_background, pdf_background,
					pdf_background);
//			printf("  page_bottom=%d\n", blank);
		}
	}

//	printf("[MUPDF] page %d: scale=%d/%d offset=%d/%d size=%dx%d\n",
//			pdf_state.page_no, pdf_state.scale_x, pdf_state.scale_y,
//			pdf_state.page_left, pdf_state.page_top, page->pixmap_width,
//			page->pixmap_height);

	return 1;
}

void pdf_scroll(bitmap_t *canvas, int scroll_x, int scroll_y) {
//	printf("scroll: %d,%d\n", scroll_x, scroll_y);

	pdf_page_t *page = pdf_pages[0];
	int page_no = pdf_run.page_no;
	int page_top = pdf_run.page_top + scroll_y;
	if (scroll_y != 0 && page->pixmap_height > 0) {
		if (page_top >= page->pixmap_height) {
			page_no += page_top / page->pixmap_height;
			if (page_no > pdf_page_count) {
				page_no = pdf_page_count;
				page_top = page->pixmap_height - pdf_line_height;
			} else {
				page_top = page_top % page->pixmap_height;
			}
		} else if (page_top < 0) {
			page_no -= ceil(-page_top / (float) page->pixmap_height);
			if (page_no < 1) {
				page_no = 1;
				page_top = 0;
			} else {
				page_top = page->pixmap_height
						- (-page_top % page->pixmap_height);
			}
		}
	}

	pdf_run.page_no = page_no;
	pdf_run.page_left[page_no % 2] = pdf_run.page_left[page_no % 2] + scroll_x;
	pdf_run.page_top = page_top;
//	printf("page_left %d (%d %d)\n", page_no % 2, pdf_run.page_left[0],
//			pdf_run.page_left[1]);
	pdf_paint(canvas);
}

void pdf_scale(bitmap_t *canvas, int scale_x, int scale_y, int center_x,
		int center_y) {
	int scale_x_new = pdf_run.scale_x * scale_x / 100.0;
	int scale_y_new = pdf_run.scale_y * scale_y / 100.0;
	if (scale_x_new < 20 || scale_y_new < 20 || scale_x_new > 500
			|| scale_y_new > 500) {
		printf("pdf_scale: %d/%d %d/%d\n", pdf_run.scale_x, scale_x,
				pdf_run.scale_y, scale_y);
		return;
	}

	pdf_run.scale_x = scale_x_new;
	pdf_run.scale_y = scale_y_new;

	//     |<-      center_x     ->|
	//     |<- offset_x ->|        +
	// after scale:
	// |<-      center_x_new     ->|
	// |<- offset_x_new ->|        +

	int center_x_new = center_x * scale_x / 100.0;
	int center_y_new = center_y * scale_y / 100.0;
	pdf_run.page_left[pdf_run.page_no % 2] += center_x_new - center_x;
	pdf_run.page_top += center_y_new - center_y;

	pdf_paint(canvas);
}

static inline char fz_rect_contains(fz_rect *rect, float x, float y) {
	return rect->x0 <= x && x <= rect->x1 && rect->y0 <= y && y <= rect->y1;
}

static inline char is_split_char(char c) {
	return (' ' <= c && c < 'A') || ('Z' < c && c < 'a')
			|| ('z' < c && c <= '~');
}

int pdf_get_text(bitmap_t *canvas, int x, int y, char *str, int len) {
	if (str == NULL || len < 1)
		return 0;

	pdf_page_t *page = NULL;
//	if (pdf_pages[0]->run.page_no > pdf_pages[1]->run.page_no) {
//		page = pdf_pages[0];
//		pdf_pages[0] = pdf_pages[1];
//		pdf_pages[1] = page;
//	}

	str[0] = 0;
	float rx, ry;
	int i;
	for (i = 0; i < PDF_PAGE_CACHE_SIZE; i++) {
		page = pdf_pages[i];
		if (page) {
//			printf("page %d: %d %d\n", page->run.page_no,
//					page->run.page_left[pdf_run.page_no % 2],
//					page->run.page_top);
			int h = page->pixmap_height - page->run.page_top;
			if (y <= h) {
				rx = page->run.page_left[page->run.page_no % 2] + x;
				ry = page->run.page_top + y;
				break;
			}
			y -= h;
		}
	}

	if (i == PDF_PAGE_CACHE_SIZE)
		return 0;

	rx = rx / page->run.scale_x * 100;
	ry = ry / page->run.scale_y * 100;
//	printf("%d,%d > %.0f,%.0f (%d,%dx%d,%d)\n", x, y, rx, ry,
//			page->run.page_left[pdf_run.page_no % 2], page->run.page_top,
//			page->run.scale_x, page->run.scale_y);

	if (pdf_sheet == NULL)
		pdf_sheet = fz_new_text_sheet(pdf_ctx);

	fz_text_page *text = fz_new_text_page(pdf_ctx);
	fz_device *dev = fz_new_text_device(pdf_ctx, pdf_sheet, text);
	fz_run_page(pdf_doc, page->page, dev, &fz_identity, NULL);
	fz_free_device(dev);

	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	for (i = 0; i < text->len; i++) {
		if (text->blocks[i].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = text->blocks[i].u.text;
		if (fz_rect_contains(&block->bbox, rx, ry)) {
			for (line = block->lines; line < block->lines + block->len;
					line++) {
				if (fz_rect_contains(&line->bbox, rx, ry)) {
					for (span = line->first_span; span; span = span->next) {
						if (fz_rect_contains(&span->bbox, rx, ry)) {
							i = -1;
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}

	int j = 0;
	if (i == -1) {
		for (j = 0; j < span->len; j++) {
			if (span->text[j].p.x > rx)
				break;
		}
		j--;
		// this is just the clicked char
		for (; j >= 0; j--) {
			if (is_split_char(span->text[j].c))
				break;
		}
		j++;
		for (i = j; i < span->len && i - j < len - 1; i++) {
			if (is_split_char(span->text[i].c))
				break;
			str[i - j] = span->text[i].c;
		}
		str[i - j] = 0;
	} else {
		i = 0; //  for return
	}

	fz_free_text_page(pdf_ctx, text);
	return i - j;
}

//	if (page_no == pdf->page_no) {
//		page = pdf->page;
//		bounds = pdf->bounds;
//		if () {
//			pixmap = pdf->pixmap;
//			if (offset_x == pdf->offset_x && offset_y == pdf->offset_y) // TODO check dest_x,y
//				return;
//		}
//	} else {
//		fz_free_page(pdf_doc, pdf->page);
//
//		page = fz_load_page(pdf_doc, page_no - 1);
//		fz_bound_page(pdf_doc, page, &bounds);
//
//		pdf->page = page;
//		pdf->bounds = bounds;
//	}

//	if (offset_y >= pdf->page_height) {
//		offset_y = pdf->page_height - MARGIN_Y;
//		if (pdf->page_height > window_height)
//			offset_y = pdf->page_height - window_height;
//		else
//			offset_y = 0;
//	} else if (offset_y < 0) {
//		offset_y = 0;
//	}

//rect_s rect;
//		rect.left = block->bbox.x0 * page->run.scale_x / 100.0;
//		rect.top = block->bbox.y0 * page->run.scale_y / 100.0;
//		rect.width = (block->bbox.x1 - block->bbox.x0) * page->run.scale_x
//				/ 100.0;
//		rect.height = (block->bbox.y1 - block->bbox.y0) * page->run.scale_y
//				/ 100.0;
//		bitmap_draw_rect(canvas, &rect);
