#ifndef WL_FREETYPE_H
#define WL_FREETYPE_H

#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <ftcache.h>
#include <ftlcdfil.h>
#include <ftoutln.h> // embolden
#include <ftbitmap.h> // embolden
#include <ftmodapi.h>

#define FT_FACE_MAIN 1
#define FT_FACE_FALLBACK 0

//extern FT_Face ft_face;
//extern FTC_ScalerRec ft_scaler;
//extern FT_Size ft_size;
//extern int avg_char_width_26_6;

extern FT_Library ft_library;
extern FTC_Manager ft_manager;
extern FTC_ImageCache ft_cache;

//typedef struct {
//	FT_Face face;
//	FT_Size size;
//	FTC_ScalerRec scaler;
//	uint32_t avg_char_width_26_6;
//} ft_font_t;

extern FT_Face ft_face_main;
extern FT_Face ft_face_fallback;

//int font_width, int font_height

void freetype_free();
int freetype_init(int max_faces, int max_sizes, int max_bytes,
		char use_lcd_filter);
FT_Face freetype_load_font(const char *font_path, int face_index);
int freetype_font(const char *font_path, int face_index,
		const char *font_path_fallback, int face_index_fallback);
FT_BitmapGlyph freetype_get_bitmap_glyph(FT_ULong code, FT_Face face,
		FTC_Scaler scaler);

// src - forground color, dst - background color
inline unsigned char alpha_blend(unsigned char dst, unsigned char src,
		unsigned char alpha255);

#endif

//extern FT_Library ftLib;
//extern FTC_Manager ftMgr;
//extern FTC_ImageCache ftCache;

//int init_freetype(const char* fontPath, int maxFaces, int maxSizes, int maxBytes);
//void free_freetype();
//FT_BitmapGlyph get_bitmap_glyph(FT_Face face, FT_ULong code, FTC_Scaler scaler);

//	FT_GlyphSlot slot = ft_face->glyph;

//	ftScaler->face_id = 0;
//	ftScaler->pixel = 1;
//	ftScaler->width = 0;
//	ftScaler->height = 0;
//	delete ftScaler;
