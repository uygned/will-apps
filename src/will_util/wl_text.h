#ifndef WL_TEXT_H
#define WL_TEXT_H

#include <wchar.h>
#include "wl_bitmap.h"

#define USE_FT_CACHE
#define USE_FT_RASTER
#define FONT_SUBPIXEL_RENDERING

#ifdef USE_FT_CACHE
#include "wl_ftcache.h"
#else
#include "wl_freetype.h"
#endif

#ifdef FONT_SUBPIXEL_RENDERING
#define FONT_SUBPIXEL_RGB 1
#define FONT_SUBPIXEL_BGR 2
#define FONT_SUBPIXEL_SCALE 3
extern unsigned char subpixel_filter[];
#else
#define FONT_SUBPIXEL_SCALE 1
#endif

typedef struct {
	FT_Face face;
	FT_Face fallback;
	FTC_ScalerRec scaler;
	FTC_ScalerRec scaler_fallback;
	uint32_t x_ppem_26_6;

	short font_height;
	short ascender;
	short descender;
	short line_spacing;

	color_t color;

	// in 26.6 pixel format (unit of 1/64 pixel)
	short embolden_x;
	short embolden_y;

	int kerning_mode; // FT_KERNING_DEFAULT
	FT_ULong load_flags;
	FT_Render_Mode render_mode;
} font_conf_t;

typedef struct {
	int curr_pos;
	int last_glyph;
	int offset_x_26_6;
	int offset_y;
	int overflowed;
} glyph_run_t;

void font_init(font_conf_t *font, int font_height, int line_spacing);

void glyph_run_init(glyph_run_t *grun, int offset_x, int offset_y);

void glyph_draw(bitmap_t *canvas,
#ifdef USE_FT_CACHE
		FT_BitmapGlyph glyph,
#else
		FT_GlyphSlot glyph,
#endif
		color_t *color, int offset_x, int offset_y);

void text_draw(bitmap_t *canvas, const wchar_t *text, font_conf_t *font,
		rect_s *rect, glyph_run_t *grun);

wchar_t *mbs2wcs(const char *src);
char *wcs2mbs(const wchar_t *src);

#endif
