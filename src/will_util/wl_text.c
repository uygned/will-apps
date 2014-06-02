#include "wl_text.h"
//#include FT_STROKER_H

void glyph_run_init(glyph_run_t *grun, int offset_x, int offset_y) {
	grun->curr_pos = 0;
	grun->last_glyph = 0;
	grun->offset_x_26_6 = offset_x << 6;
	grun->offset_y = offset_y;
	grun->overflowed = 0;
}

void font_init(font_conf_t *font, int font_height, int line_spacing) {
	if (font == NULL)
		return;

// https://freddie.witherden.org/pages/font-rasterisation/
// the slight style hints just the y-axis.
// it incurs no character drift whatsoever,
// while still providing an improvement over regular anti-aliasing.
#ifdef KINDLE
	font->load_flags = FT_LOAD_TARGET_NORMAL;
	font->render_mode = FT_RENDER_MODE_NORMAL;
#else
	font->load_flags = FT_LOAD_TARGET_LIGHT;
	font->render_mode = FT_RENDER_MODE_LCD;
#endif

	font->kerning_mode = 0;

	font->face = ft_face_main;
	font->fallback = ft_face_fallback;

	font->scaler.face_id = (FTC_FaceID) FT_FACE_MAIN;
	font->scaler.pixel = 1;
	font->scaler.width = 0;
	font->scaler.height = font_height;
	font->scaler_fallback.face_id = (FTC_FaceID) FT_FACE_FALLBACK;
	font->scaler_fallback.pixel = 1;
	font->scaler_fallback.width = 0;
	font->scaler_fallback.height = font_height;

#ifdef USE_FT_RASTER
#ifdef FONT_SUBPIXEL_RENDERING
	if (font->render_mode == FT_RENDER_MODE_LCD) {
		font->scaler.width = font_height * FONT_SUBPIXEL_SCALE;
		font->scaler_fallback.width = font_height * FONT_SUBPIXEL_SCALE;
	}
#endif
#endif

	FT_Size ft_size;
	FTC_Manager_LookupSize(ft_manager, &font->scaler, &ft_size);
	printf(
			"[FREETYPE]: intput_height=%d ascender=%.0f descender=%.0f y_ppem=%d x_ppem=%d max_advance=%.0f\n",
			font_height, ft_size->metrics.ascender / 64.0,
			ft_size->metrics.descender / 64.0, ft_size->metrics.y_ppem,
			ft_size->metrics.x_ppem, ft_size->metrics.max_advance / 64.0);
	font->ascender = ft_size->metrics.ascender / 64.0 + 0.5;
	font->descender = abs(ft_size->metrics.descender) / 64.0 + 0.5;
	font->x_ppem_26_6 = ft_size->metrics.x_ppem << 6;

//	FT_UInt glyph_index = FT_Get_Char_Index(font->face, (FT_ULong) ' ');
//	if (glyph_index) {
//		if (FT_Load_Glyph(font->face, glyph_index, FT_LOAD_DEFAULT) == 0)
//			FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL);
//		font->x_ppem_26_6 = font->face->glyph->metrics.horiAdvance;
//		printf("[FONT] font_height=%d avg_char_width=%d\n", font_height,
//				font->x_ppem_26_6 >> 6);
//	}
//
//	if (font->x_ppem_26_6 == 0)
//		font->x_ppem_26_6 = font_height << 6;

	font->color.r = 0;
	font->color.g = 0;
	font->color.b = 0;
	font->font_height = font_height;
	font->line_spacing = line_spacing;
	font->embolden_x = 0;
	font->embolden_y = 0;
}

#ifdef USE_FT_RASTER

#ifdef FONT_SUBPIXEL_RENDERING
typedef struct {
	bitmap_t bitmap;
	color_t *color;
	ushort pixel_geometry;
	char subpixel_offset;
} ftr_run_t;
unsigned char subpixel_filter[] = { 15, 60, 105, 60, 15 };
#else
typedef struct {
	bitmap_t bitmap;
	color_t *color;
}ftr_run_t;
#endif

//#ifdef FONT_SUBPIXEL_RENDERING
//unsigned char m_primary[256];
//unsigned char m_secondary[256];
//unsigned char m_tertiary[256];
//void fir_filter(float prim, float second, float tert) {
//	for (unsigned i = 0; i < 256; i++) {
//		m_primary[i] = (unsigned char) floor(prim * i);
//		m_secondary[i] = (unsigned char) floor(second * i);
//		m_tertiary[i] = (unsigned char) floor(tert * i);
//	}
//}
//#endif

//int min_x = INT_MAX;
//int max_x = 0;

// see gray_render_span in smooth/ftgrays.c
void ft_gray_spans(int y, int count, const FT_Span *spans, void *user) {
	ftr_run_t *args = user;
	color_t *rgb = args->color;
	bitmap_t *dst = &args->bitmap;
//	if (y > dst->rect.top)
	int y_flip = dst->rect.top - y; // dst->rect.top with font-ascender added
	if (y_flip < 0 || y_flip >= dst->rect.top + dst->rect.height)
		return;

	const FT_Span *span = spans;
	unsigned char *data = dst->data + y_flip * dst->bytes_per_line
			+ dst->rect.left * dst->bytes_per_pixel;
	int i, j;

#ifdef FONT_SUBPIXEL_RENDERING
	if (args->pixel_geometry > 0 && dst->bytes_per_pixel >= 3) {
		const FT_Span *last = span + count - 1;
		int first_subpixel = span->x;
		uint32_t width = last->x + last->len - first_subpixel + 4;
		unsigned char *filtered = (unsigned char *) calloc(1, width);

		int k;
		for (i = 0; i < count; i++, span++) {
//			if (span->x < min_x)
//				min_x = span->x;
//			if (span->x + span->len > max_x)
//				max_x = span->x;

			unsigned start = span->x - first_subpixel;
			unsigned end = start + span->len;
			for (j = start; j < end; j++) {
				for (k = 0; k < 5; k++) {
					float f = filtered[j + k]
							+ span->coverage * subpixel_filter[k] / 255.0;
					filtered[j + k] = f > 255 ? 255 : f;
				}
			}
		}

		i = 0;
		first_subpixel += -2/*FIR5*/+ args->subpixel_offset;
		//		while (first_subpixel < 0
		//				&& dst->rect.left + (first_subpixel - 2) / FONT_SUBPIXEL_SCALE < 0) {
		//			first_subpixel++;
		//			i++;
		//		}
//			if (first_subpixel < dst->rect.left * FONT_SUBPIXEL_SCALE)
//				first_subpixel = 0;

		int pixel = 0;
		unsigned subpixel = 0;
		if (first_subpixel > 0) {
			pixel = first_subpixel / FONT_SUBPIXEL_SCALE;
			subpixel = first_subpixel % FONT_SUBPIXEL_SCALE;
		} else if (first_subpixel < 0) {
			//    pixle layout:      |    -2    |    -1    |   0   |
			// subpixle layout:      | -6 -5 -4 | -3 -2 -1 | 0 1 2 |
			first_subpixel -= 2;
			// subpixle layout (-2): | -8 -7 -6 | -5 -4 -3 |
			pixel = first_subpixel / FONT_SUBPIXEL_SCALE;
			subpixel = first_subpixel % FONT_SUBPIXEL_SCALE + 2;
		}
		unsigned char *p = data + pixel * dst->bytes_per_pixel;
		unsigned char alpha;
		for (; i < width; i++) { // TODO width
			alpha = filtered[i];
			if (args->pixel_geometry == FONT_SUBPIXEL_RGB) {
				if (subpixel == 2) { // subpixel compoment B
					p[0] = alpha_blend(p[0], rgb->b, alpha);
				} else if (subpixel == 1) { // subpixel compoment G
					p[1] = alpha_blend(p[1], rgb->g, alpha);
				} else { // subpixel compoment R
					p[2] = alpha_blend(p[2], rgb->r, alpha);
				}
			} else if (args->pixel_geometry == FONT_SUBPIXEL_BGR) {
				if (subpixel == 2) { // subpixel compoment R
					p[2] = alpha_blend(p[2], rgb->r, alpha);
				} else if (subpixel == 1) { // subpixel compoment G
					p[1] = alpha_blend(p[1], rgb->g, alpha);
				} else { // subpixel compoment B
					p[0] = alpha_blend(p[0], rgb->b, alpha);
				}
			} else {
				perror("invalid pixel_geometry");
			}

			if (++subpixel == FONT_SUBPIXEL_SCALE) {
				subpixel = 0;
				p += dst->bytes_per_pixel;
			}
		}

		free(filtered);
		return;
	}
#endif
	for (i = 0; i < count; i++, span++) {
//		printf("y=%d %d span: %d %d\n", y, bmp->rect.top, span->x, span->len);
		int l = min(span->len, dst->rect.width - span->x);
		if (l <= 0)
			break;
		unsigned char *p = data + span->x * dst->bytes_per_pixel;
		unsigned char alpha = span->coverage;
		for (j = 0; j < l; j++) {
			p[0] = alpha_blend(p[0], rgb->b, alpha);
			if (dst->bytes_per_pixel == 3) {
				p[1] = alpha_blend(p[1], rgb->g, alpha);
				p[2] = alpha_blend(p[2], rgb->r, alpha);
			}
			p += dst->bytes_per_pixel;
		}
	}
}
#endif /* USE_FT_RASTER */

void text_draw(bitmap_t *canvas, const wchar_t *text, font_conf_t *font,
		rect_s *rect, glyph_run_t *grun) {
	if (!canvas || !text || !font)
		return;

	glyph_run_t *grun_ = NULL;
	if (grun == NULL) {
		grun = malloc(sizeof(glyph_run_t));
		glyph_run_init(grun, 0, 0);
		grun_ = grun;
	}

	// in 26.6 fractional pixels
	int left_26_6 = (rect->left << 6) * FONT_SUBPIXEL_SCALE;
	int right_26_6 = left_26_6 + (rect->width << 6) * FONT_SUBPIXEL_SCALE;
	int x_26_6 =
			grun->offset_x_26_6 != 0 ?
					(grun->offset_x_26_6 * FONT_SUBPIXEL_SCALE) : left_26_6;

	if (grun->offset_y == 0)
		grun->offset_y = rect->top;

//	printf("text_draw: T%d H%d L%d W%d Y%d X%d F%d %ls\n", rect->top,
//			rect->height, rect->left, rect->width, grun->offset_y,
//			offset_x_26_6, font->font_height, text);

	grun->offset_y += font->ascender + font->descender;
	if (grun->offset_y > rect->top + rect->height) {
		grun->overflowed = 1;
		if (grun_)
			free(grun_);
		return;
	}

#ifdef USE_FT_RASTER
	FT_Raster_Params ftr_par;
	ftr_par.target = 0;
	ftr_par.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA;
	ftr_par.gray_spans = ft_gray_spans;
	ftr_par.black_spans = 0;
	ftr_par.bit_test = 0;
	ftr_par.bit_set = 0;

	ftr_run_t ftr_run;
	ftr_par.user = &ftr_run;
	ftr_run.bitmap.data = canvas->data;
	ftr_run.bitmap.bytes_per_line = canvas->bytes_per_line;
	ftr_run.bitmap.bytes_per_pixel = canvas->bytes_per_pixel;
	ftr_run.color = &font->color;
#ifdef FONT_SUBPIXEL_RENDERING
	if (font->render_mode == FT_RENDER_MODE_LCD)
		ftr_run.pixel_geometry = FONT_SUBPIXEL_RGB;
	else
		ftr_run.pixel_geometry = 0;
#endif
#endif

	int destroy_glyph = 0;
	FT_UInt glyph_index = 0;
	int glyph_advance = 0;
	FT_Glyph glyph = NULL;
	FT_Vector kerning;
	FT_Error err = 0;
	int pos = grun->curr_pos;
	size_t len = wcslen(text);
	while (pos < len) {
		wchar_t c = text[pos];
		if (c == 0) // end of wstring
			break;

		if ((c == '\r' || c == '\n')
				|| (x_26_6 + font->x_ppem_26_6 * FONT_SUBPIXEL_SCALE
						> right_26_6)) {
			// line break or line wrapping
			pos++;
			if (c == '\r' && pos + 1 < len && text[pos + 1] == '\n')
				pos++;

			grun->last_glyph = 0;
			x_26_6 = left_26_6;
			grun->offset_y += font->line_spacing + font->ascender
					+ font->descender;
			if (grun->offset_y > rect->top + rect->height) {
				grun->overflowed = 1;
				break;
			}

			continue;
		}

		FT_Face ft_face = font->face;
		FTC_Scaler scaler = &font->scaler;

#ifdef USE_FT_CACHE
		glyph_index = FT_Get_Char_Index(ft_face, (FT_ULong) c);
		if (glyph_index == 0) {
			ft_face = font->fallback;
			scaler = &font->scaler_fallback;
			glyph_index = FT_Get_Char_Index(ft_face, (FT_ULong) c);
			if (glyph_index == 0) {
				// TODO draw something
				printf("[FONT] glyph_index 0: `%lc'\n", text[pos]);
				goto END;
			}
		}

		if (font->kerning_mode) {
			if (FT_Get_Kerning(ft_face, grun->last_glyph, glyph_index,
					font->kerning_mode, &kerning) == 0)
				x_26_6 += kerning.x * FONT_SUBPIXEL_SCALE;
			grun->last_glyph = glyph_index;
		}

		destroy_glyph = 0;
		if (FTC_ImageCache_LookupScaler(ft_cache, scaler, font->load_flags,
				glyph_index, &glyph, NULL)) {
			perror("FTC_ImageCache_LookupScaler");
			goto END;
		}

		if (font->embolden_x != 0 || font->embolden_y != 0) {
			FT_Glyph g;
			if (FT_Glyph_Copy(glyph, &g) != 0) {
				if (g)
					FT_Done_Glyph(g);
				perror("FT_Glyph_Copy");
				goto END;
			}

			if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
				FT_Bitmap bitmap = ((FT_BitmapGlyph) g)->bitmap;
				err = FT_Bitmap_Embolden(ft_library, &bitmap,
						(FT_Pos) font->embolden_x, (FT_Pos) font->embolden_y);
			} else if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
				FT_Outline outline = ((FT_OutlineGlyph) g)->outline;
				err = FT_Outline_EmboldenXY(&outline, (FT_Pos) font->embolden_x,
						(FT_Pos) font->embolden_y);
			}

			if (err) {
				FT_Done_Glyph(g);
				printf("[ERROR] FT_Embolden\n");
				goto END;
			}

			glyph = g;
			destroy_glyph = 1;
		}

		glyph_advance = 0;

#ifdef USE_FT_RASTER
		if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
//			FT_Stroker stroker;
//			FT_Stroker_New(ft_library, &stroker);
//			FT_Stroker_Set(stroker, (int) 32,
//					FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
//			FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1);
//			FT_Stroker_Done(stroker);

			FT_OutlineGlyph o = (FT_OutlineGlyph) glyph;
			FT_Outline *outline = &o->outline;

			ftr_run.bitmap.rect.left = x_26_6 / 64.0 + 0.5;
#ifdef FONT_SUBPIXEL_RENDERING
			ftr_run.subpixel_offset = ftr_run.bitmap.rect.left
					% FONT_SUBPIXEL_SCALE;
			ftr_run.bitmap.rect.left /= FONT_SUBPIXEL_SCALE;
#endif
			ftr_run.bitmap.rect.width = canvas->rect.left + canvas->rect.width
					- ftr_run.bitmap.rect.left;
			ftr_run.bitmap.rect.top = grun->offset_y - font->descender;
			ftr_run.bitmap.rect.height = canvas->rect.top + canvas->rect.height
					- ftr_run.bitmap.rect.top;
			ftr_par.source = outline;
			FT_Outline_Render(ft_library, outline, &ftr_par);
			glyph_advance = o->root.advance.x >> 10; // 16.16 -> 26.6
//			if (c != L' ')
//				printf("%lc: advance=%d.%d %d-%d offset=%d.%d\n", c,
//						(glyph_advance >> 6) / 3, (glyph_advance >> 6) % 3,
//						min_x, max_x, ftr_run.bitmap.rect.left,
//						ftr_run.subpixel_offset);
//			min_x = INT_MAX;
//			max_x = 0;
		} else
#endif
		if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
			FT_Glyph g = glyph;
			// This function does nothing if the glyph format isn't scalable.
			if (FT_Glyph_To_Bitmap(&glyph, font->render_mode, NULL,
					destroy_glyph)) {
				perror("FT_Glyph_To_Bitmap");
				goto END;
			}
			destroy_glyph = glyph != g;
		}
		if (glyph->format == FT_GLYPH_FORMAT_BITMAP) {
			FT_BitmapGlyph b = (FT_BitmapGlyph) glyph;
			glyph_draw(canvas, b, &font->color, x_26_6 / 64.0 + 0.5,
					grun->offset_y - font->descender);
			glyph_advance = b->root.advance.x >> 10; // 16.16 -> 26.6
		}

		END: pos++;
//#ifdef USE_FT_RASTER
//#ifdef FONT_SUBPIXEL_RENDERING
//		if (font->render_mode == FT_RENDER_MODE_LCD)
//			glyph_advance = glyph_advance / FONT_SUBPIXEL_SCALE;
//#endif
//#endif
//		printf("char %lc: left=%d/%f/%d subpixel=%d advance=%f/%d\n", c,
//				raster_args.bitmap.rect.left, offset_x_26_6 / 64.0,
//				offset_x_26_6, raster_args.subpixel_offset,
//				glyph_advance / 64.0, glyph_advance);
		x_26_6 +=
				glyph_advance != 0 ?
						glyph_advance :
						(font->x_ppem_26_6 * FONT_SUBPIXEL_SCALE);
		if (destroy_glyph)
			FT_Done_Glyph(glyph);
#else
		glyph_draw(ft_slot, offset_x, offset_y);
		x_26_6 +=
		ft_slot.advance.x == 0 ? avg_char_width_16 : ft_slot.advance.x; // 26.6
#endif
	}

//#ifdef FONT_SUBPIXEL_RENDERING
//	int i;
//	unsigned char *data = canvas->data + rect->top * canvas->bytes_per_line
//			+ rect->left * canvas->bytes_per_pixel;
//	for (i = rect->left; i < rect->width; i += 3, data += 9) {
//		data[0] = 0;
//		data[1] = 0;
//		data[2] = 255;
//		data[3] = 0;
//		data[4] = 255;
//		data[5] = 0;
//		data[6] = 255;
//		data[7] = 0;
//		data[8] = 0;
//	}
//#endif

	if (grun_) {
		free(grun_);
	} else {
		grun->curr_pos = pos;
		grun->offset_x_26_6 = x_26_6 / FONT_SUBPIXEL_SCALE;
	}
}

//static void text_draw2(bitmap_t *canvas, rect_s *rect, wchar_t *text,
//		font_conf_t *font) {
//	text_run_t *trun = text_run_new();
//	text_draw(canvas, rect, text, font, trun);
//	free(trun);
//}

void glyph_draw(bitmap_t *canvas,
#ifdef USE_FT_CACHE
		FT_BitmapGlyph glyph,
#else
		FT_GlyphSlot glyph,
#endif
		color_t *color, int offset_x, int offset_y) {

	if (!glyph)
		return;

	char monochrome = glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO;

	int src_pixel_size = 1;
	if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_LCD
			|| glyph->bitmap.pixel_mode == FT_PIXEL_MODE_LCD_V)
		src_pixel_size = 3;
	int src_line_size = glyph->bitmap.pitch;
	int src_w = glyph->bitmap.width / src_pixel_size;
	int src_h = glyph->bitmap.rows;

	int dst_pixel_size = canvas->bytes_per_pixel;
	int dst_line_size = canvas->bytes_per_line;
	int dst_w = canvas->rect.width;
	int dst_h = canvas->rect.height;

//	printf("draw %d,%d monochrome=%d src_pixel_size=%d %d src_width=%d,%d\n",
//			offset_x, offset_y, monochrome, src_pixel_size, src_line_size,
//			src_w, src_h);

	int src_x = 0;
	int src_y = 0;

#ifdef USE_FT_CACHE
	int dst_x = offset_x + glyph->left;
	int dst_y = offset_y - glyph->top;
#else
	int dst_x = offset_x + glyph->bitmap_left;
	int dst_y = offset_y - glyph->bitmap_top;
#endif

	if (dst_x < 0) {
		src_x += -dst_x; // clip left
		dst_x = 0;
	}
	if (dst_y < 0) {
		src_y += -dst_y; // clip top
		dst_y = 0;
	}

	unsigned char *src_line = src_line_size > 0 ? glyph->bitmap.buffer // start of the first row
			: glyph->bitmap.buffer + (src_h - 1) * -src_line_size; // start of the last row
	unsigned char *dst_line = canvas->data;
	src_line += src_y * src_line_size + src_x * src_pixel_size;
	dst_line += dst_y * dst_line_size + dst_x * dst_pixel_size;

	for (; src_y < src_h && dst_y < dst_h; src_y++, dst_y++) {
		unsigned char* src = src_line;
		unsigned char* dst = dst_line;
		int s = src_x;
		int d = dst_x;
		if (monochrome) {
			for (; s < src_w && d < dst_w; s++, d++) {
				// http://skia.googlecode.com/svn/trunk/src/ports/SkFontHost_FreeType_common.cpp
				int low_bit = src_line[s >> 3] >> (~s & 7);
				if (low_bit & 1) {
					// dst = alpha * src + (1 - alpha) * dst
					dst[0] = color->b;
					dst[1] = color->g;
					dst[2] = color->r;
				}
				dst += dst_pixel_size;
			}
		} else {
			for (; s < src_w && d < dst_w; s++, d++) {
				if (src_pixel_size == 1 && dst_pixel_size == 1) {
					dst[0] = alpha_blend(dst[0], color->r, src[0]);
				} else if (src_pixel_size == 1 && dst_pixel_size == 3) {
					unsigned char alpha = src[0];
					dst[0] = alpha_blend(dst[0], color->b, alpha);
					dst[1] = alpha_blend(dst[1], color->g, alpha);
					dst[2] = alpha_blend(dst[2], color->r, alpha);
				} else if (src_pixel_size == 3 && dst_pixel_size == 3) {
					dst[0] = alpha_blend(dst[0], color->b, src[2]);
					dst[1] = alpha_blend(dst[1], color->g, src[1]);
					dst[2] = alpha_blend(dst[2], color->r, src[0]);
				}
				src += src_pixel_size;
				dst += dst_pixel_size;
			}
		}
		src_line += src_line_size;
		dst_line += dst_line_size;
	}
}

//FT_Bitmap *ft_bitmap_new(int width, int height, char pixel_mode) {
////	FT_Bitmap *bitmap = ML_NEW(FT_Bitmap);
////	if (bitmap == NULL) {
////		return NULL;
////	}
//
//	int size = width * height;
//	unsigned char *buffer = calloc(1, size);
//	if (buffer == NULL)
//		return NULL;
//
//	FT_Bitmap *bitmap;
//	FT_Bitmap_New(bitmap);
//	bitmap->buffer = buffer;
//	bitmap->pitch = width;
//	bitmap->width = width;
//	bitmap->rows = height;
//	bitmap->pixel_mode = pixel_mode;
//	if (pixel_mode == FT_PIXEL_MODE_GRAY)
//		bitmap->num_grays = 256;
//	return bitmap;
//}

// http://man7.org/linux/man-pages/man3/mbstowcs.3.html
wchar_t *mbs2wcs(const char *src) {
	if (!src)
		return NULL;

	size_t len = mbstowcs(NULL, src, 0);
	if (len == (size_t) -1) // Invalid or incomplete multibyte or wide character
		return NULL;

	wchar_t *dst = malloc((len + 1) * sizeof(wchar_t));
	if (dst == NULL) {
		printf("mbs2wcs malloc error");
		return NULL;
	}

	if (mbstowcs(dst, src, len + 1) == (size_t) -1) {
		free(dst);
		return NULL;
	}

	return dst;
}

char *wcs2mbs(const wchar_t *src) {
	if (!src)
		return NULL;

	size_t len = wcstombs(NULL, src, 0);
	if (len == (size_t) -1)
		return NULL;

	char *dst = malloc((len + 1) * sizeof(char));
	if (dst == NULL) {
		printf("wcs2mbs malloc error");
		return NULL;
	}

	if (wcstombs(dst, src, len + 1) == (size_t) -1) {
		free(dst);
		return NULL;
	}

	return dst;
}

//	FT_Bitmap *bitmap;
//	FT_Bitmap_New(bitmap);
//	bitmap->pitch = font->font_height;
//	bitmap->width = font->font_height;
//	bitmap->rows = font->font_height;
//
//	int size = bitmap->pitch * bitmap->rows;
//	unsigned char *buffer = calloc(1, size);
//	if (buffer == NULL)
//		return;
//	bitmap->buffer = buffer;
//
//	FT_Raster_Params ftr_params;
//	memset(&ftr_params, 0, sizeof(ftr_params));
//
//#ifdef KINDLE
////	ftr_params.target = ft_bitmap_new(font->font_height, font->font_height);
//	bitmap->pixel_mode = FT_PIXEL_MODE_GRAY;
//	bitmap->num_grays = 256;
//#else
////	ftr_params.target = ft_bitmap_new(font->font_height, font->font_height);
//	bitmap->pixel_mode = FT_PIXEL_MODE_LCD;
//#endif

//for (i = 0; i < count; i++, span++) {
//			ushort pix = span->x / FONT_SUBPIXEL_SCALE;
//			ushort sub = span->x % FONT_SUBPIXEL_SCALE;
//			printf("y=%d span: %d %d %d %d %d\n", y, span->x, span->len,
//					span->coverage, pix, sub);
//			unsigned char *d = data + pix * bmp->bytes_per_pixel;
//			int l = min(span->len, bmp->rect.width * FONT_SUBPIXEL_SCALE - span->x);
//			if (l <= 0)
//				break;
//			unsigned char alpha = span->coverage;
////			unsigned char alpha = 255;
//			for (j = 0; j < l; j++) {
////				if (i == 0 && j == 0 && sub == 2) {
////					// when bg is white (255, 255, 255)
////					d[0] = 255;
////					d[1] = 0;
////					d[2] = 0;
////					alpha = 0;
////				}
////				unsigned char alpha = line[span->x + j];
//				if (line)
//					alpha = line[filter + span->x + j];
//				if (args->pixel_geometry == SUBPIXEL_RGB) {
//					if (sub == 2) // subpixel compoment B
//						d[0] = alpha_blend(d[0], clr->b, alpha);
//					else if (sub == 1) // subpixel compoment G
//						d[1] = alpha_blend(d[1], clr->g, alpha);
//					else
//						// subpixel compoment R
//						d[2] = alpha_blend(d[2], clr->r, alpha);
//				} else
