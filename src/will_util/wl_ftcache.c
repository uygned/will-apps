#include "wl_ftcache.h"
#include <stdint.h>

inline unsigned char alpha_blend(unsigned char dst, unsigned char src,
		unsigned char alpha255) {
	// http://www.freetype.org/freetype2/docs/reference/ft2-lcd_filtering.html
	// dst = alpha * src + (1 - alpha) * dst (for each channel, alpha in [0, 1])
	// dst = dst + (src - dst) * alpha / 255 (alpha in [0, 255])
	return dst + (src - dst) * alpha255 / 255.0;
}

FT_Library ft_library = NULL;
FTC_Manager ft_manager = NULL;
FTC_ImageCache ft_cache = NULL;

FT_Face ft_face_main = NULL;
FT_Face ft_face_fallback = NULL; // TODO to free

//FTC_ScalerRec ft_scaler_main;
//FTC_ScalerRec ft_scaler_fallback;
//FT_Size ft_size_main = NULL;
//FT_Size ft_size_fallback = NULL;

FT_Error ft_face_requester(FTC_FaceID face_id, FT_Library library,
		FT_Pointer request_data, FT_Face *face) {
//	*face = ft_face_main;
	*face = ((intptr_t) face_id) == FT_FACE_MAIN ?
			ft_face_main : ft_face_fallback;
	return 0;
}

void freetype_free() {
//	FT_Done_Face(ft_face); // Not do this if FTC_Manager owns the face object
	if (ft_manager) {
		FTC_Manager_Done(ft_manager);
		ft_manager = NULL;
	}
	if (ft_library) {
		FT_Done_Library(ft_library);
		ft_library = NULL;
	}
	if (ft_face_main) {
		ft_face_main = NULL;
	}
	if (ft_face_fallback) {
		ft_face_fallback = NULL;
	}
}

// FTC defaults in ftcmanag.h
// FT_Size - A handle to an object used to model a face scaled to a given character size.
/*
 #define FTC_MAX_FACES_DEFAULT  2
 // Maximum number of opened FT_Size objects managed by this cache instance.
 #define FTC_MAX_SIZES_DEFAULT  4
 #define FTC_MAX_BYTES_DEFAULT  200000L
 */
int freetype_init(int max_faces, int max_sizes, int max_bytes,
		char use_lcd_filter) {
	// FT_Error error; // 0 means success.
	if (FT_Init_FreeType(&ft_library)) {
		perror("FT_Init_FreeType");
		return 0;
	}
	if (FTC_Manager_New(ft_library, max_faces, max_sizes, max_bytes,
			ft_face_requester, NULL, &ft_manager)) {
		perror("FTC_Manager_New");
		return 0;
	}
	if (FTC_ImageCache_New(ft_manager, &ft_cache)) {
		perror("FTC_ImageCache_New");
		return 0;
	}

	if (use_lcd_filter) {
		// https://freddie.witherden.org/pages/font-rasterisation/
		float f = 0x255 / 17.0;
		unsigned char weights[] = { f, f * 4, f * 7, f * 4, f };

		FT_Library_SetLcdFilter(ft_library, FT_LCD_FILTER_LIGHT);
		// This function must be called *after* FT_Library_SetLcdFilter to have any effect.
		FT_Library_SetLcdFilterWeights(ft_library, weights);
	}

	return 1;
}

FT_Face freetype_load_font(const char *font_path, int face_index) {
	FT_Face face;
	FT_Error error = FT_New_Face(ft_library, font_path, face_index, &face);
	if (error) {
		printf("[ERROR] FT_New_Face: %d\n", error);
		return NULL;
	}
	printf(
			"[FREETYPE] font `%s': ascender=%d descender=%d hinter=%s kerning=%s mono=%s upem=%d\n",
			face->family_name, face->ascender >> 6, face->descender >> 6,
			face->face_flags & FT_FACE_FLAG_HINTER ? "true" : "false",
			face->face_flags & FT_FACE_FLAG_KERNING ? "true" : "false",
			face->face_flags & FT_FACE_FLAG_FIXED_WIDTH ? "true" : "false",
			face->units_per_EM);
	return face;
}

int freetype_font(const char *font_path, int face_index,
		const char *font_path_fallback, int face_index_fallback) {
	ft_face_main = freetype_load_font(font_path, face_index);
	if (font_path_fallback) {
		ft_face_fallback = freetype_load_font(font_path_fallback,
				face_index_fallback);
	}
	return 1;
}

FT_BitmapGlyph freetype_get_bitmap_glyph(FT_ULong code, FT_Face face,
		FTC_Scaler scaler) {
	FT_UInt glyph_index = FT_Get_Char_Index(face, code);
	if (glyph_index == 0)
		return NULL;

	FT_Glyph glyph;
	if (FTC_ImageCache_LookupScaler(ft_cache, scaler, FT_LOAD_TARGET_LCD,
			glyph_index, &glyph, NULL/*node*/) != 0)
		return NULL;

	FT_Bool is_ftc_glyph = 1;
	if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_LCD, NULL,
			!is_ftc_glyph/*destroy original glyph*/) != 0) {
		if (!is_ftc_glyph)
			FT_Done_Glyph(glyph);
		return NULL;
	}

	return (FT_BitmapGlyph) glyph;
}
