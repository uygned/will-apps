#	-Wl,--as-needed -Wl,--no-as-needed
HOST ?= local
PREFIX     ?= /usr
PREFIX_X11 ?= /usr
PREFIX_GTK ?= /usr
CFLAGS := $(CFLAGS) -DHAVE_CONFIG_H -I$(PREFIX)/include/freetype2 -Wall -Wno-unused-function

ifeq ($(HOST),local)
CFLAGS := $(CFLAGS) -fPIC -g -O0 -I/oss/arch/home/pxlogpx/local/mupdf-1.4-source/include -I/usr/include/agg2
LDFLAGS := -L/oss/arch/home/pxlogpx/local/mupdf-1.4-source/build/debug
LIBS_NEEDED := -lfreetype -ljpeg
else
CFLAGS := $(CFLAGS) -I$(X11INC) -I$(PREFIX)_X11/include -DKINDLE -DNO_X11
LDFLAGS := $(LDFLAGS) -L$(PREFIX)_X11/lib -L$(KINDLE)/lib -L$(KINDLE)/usr/lib
LIBS_NEEDED := -lfreetype
endif

LIBS_X11 := -lX11 -lxcb -lXau -lXdmcp -lXt -lSM -lXaw -lXmu \
	-lXext -lXpm -lICE -luuid -lXrender
# -lmupdf-js-none -lmupdf-font
LIBS_MUPDF := -lmupdf -lz -lpng -ljpeg -lopenjpeg -ljbig2dec

# [/usr/include]$ cp -r gtk-2.0/ glib-2.0/ gdk-pixbuf-2.0/ atk-1.0/ pango-1.0/ cairo/ /arm/ARCH/opt/gtk2/
# [/usr/include]$ cp -r ../lib/glib-2.0/ ../lib/gtk-2.0/ /arm/ARCH/opt/gtk2/lib
INCS_GTK := -I$(PREFIX_GTK)/include/gtk-2.0 -I$(PREFIX_GTK)/lib/gtk-2.0/include \
	-I$(PREFIX_GTK)/include/gdk-pixbuf-2.0 -I$(PREFIX_GTK)/include/atk-1.0 \
	-I$(PREFIX_GTK)/include/glib-2.0 -I$(PREFIX_GTK)/lib/glib-2.0/include \
	-I$(PREFIX_GTK)/include/cairo -I$(PREFIX_GTK)/include/pango-1.0
#LIBS_GTK := -lglib-2.0 -lgtk-x11-2.0 -lgdk-x11-2.0 -lgdk_pixbuf-2.0 \
#	-lgobject-2.0 -lgio-2.0 -lgmodule-2.0 -lgthread-2.0 \
#	-latk-1.0 -lcairo -lpixman-1 -lpango-1.0 -lpangoft2-1.0 -lpangocairo-1.0
LIBS_CAIRO := -I$(PREFIX_GTK)/include/cairo \
	-I$(PREFIX_GTK)/include/pango-1.0 -I$(PREFIX_GTK)/include/pixman-1 \
	-I$(PREFIX_GTK)/include/glib-2.0 -I$(PREFIX_GTK)/lib/glib-2.0/include
LIBS_CAIRO := $(LIBS_CAIRO) \
	-L/oss/eclipsews/cairo-1.12.16/src/.libs \
	-lcairo -lpixman-1 -lpango-1.0 -lpangoft2-1.0 -lpangocairo-1.0 -lglib-2.0 

# http://blog.jgc.org/2012/01/using-gnu-makes-define-and-eval-to.html
define make_x11
$1: $2
	@echo "  EXE  $$@"
	$(CC) $(CFLAGS) $(LDFLAGS) $3 $$^ -o $$@
endef

INCS := -I./include -I./src/will_util -I$(PREFIX_X11)/include \
	-I$(PREFIX)/include/freetype2 -I$(PREFIX)/include/libxml2

SRCS_FONT = src/will_util/wl_bitmap.c src/will_util/wl_text.c src/will_util/wl_ftcache.c
#$(eval $(call make_x11,build/$(HOST)/test_pango,test/test_pango.c))
$(eval $(call make_x11, build/$(HOST)/will_pdf, src/will_pdf.c $(SRCS_FONT) src/will_util/grid_view.c \
	src/will_util/wl_mupdf.c src/will_util/wl_stardict.c src/will_util/wl_hashmap.c \
	src/will_util/murmur3_32.c src/will_util/porter_stemmer.c,\
	$(INCS) $(LIBS_X11) $(LIBS_MUPDF) -lfreetype -lm -lpthread))
$(eval $(call make_x11, build/$(HOST)/will_txt, src/will_txt.c $(SRCS_FONT) src/will_util/grid_view.c,\
	$(INCS) $(LIBS_X11) -lfreetype -lpthread))
$(eval $(call make_x11, build/$(HOST)/will_rss, src/will_rss.c $(SRCS_FONT) src/will_util/grid_view.c,\
	$(INCS) $(LIBS_X11) -lfreetype -lxml2))
$(eval $(call make_x11, build/$(HOST)/will_font, src/will_font.c $(SRCS_FONT),\
	$(INCS) $(LIBS_X11) -lfreetype -lm -lpthread))
$(eval $(call make_x11, build/$(HOST)/will_font_agg, src/will_font_agg.cpp $(SRCS_FONT),\
	$(INCS) $(LIBS_X11) -lstdc++ -lagg -lfreetype -lm -lpthread))
$(eval $(call make_x11, build/$(HOST)/freetype_lcd, external/freetype_lcd.cpp $(SRCS_FONT),\
	$(INCS) $(LIBS_X11) -lstdc++ -lagg -laggplatformX11 -laggfontfreetype -lfreetype -lm -lpthread))
#$(eval $(call make_x11, build/$(HOST)/will_files, src/will_files.c, $(INCS_GTK) $(LIBS_GTK)))
$(eval $(call make_x11, build/$(HOST)/test_cairo, src/test_cairo.c $(SRCS_FONT),\
	$(INCS) $(LIBS_X11) -lfreetype $(LIBS_CAIRO)))
$(eval $(call make_x11, build/$(HOST)/will_wm, src/will_wm/will_wm.c,\
	-Isrc/will_wm $(LIBS_X11)))

EXES := will_font will_wm
#EXES := will_rss will_pdf will_txt will_font
#ifeq ($(HOST),local)
#EXES := $(EXES) will_font_agg freetype_lcd test_cairo
#endif

all: build/$(HOST) $(EXES:%=build/$(HOST)/%)
	cd src/will_web && qmake-qt4 && make

%.o: src/%.c
	@echo "  CC  $@"
	@$(CC) $(CFLAGS) -o build/$(HOST)/$@ $< -c -I./include -I./src/will_util

build/$(HOST):
	mkdir -p $@

clean:
	rm -rf build/$(HOST)/*
