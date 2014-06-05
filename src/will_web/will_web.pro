MOC_DIR = .moc
OBJECTS_DIR = .obj

QT += core gui webkit network

HEADERS = GSimpleLabel.h   GProxyLabel.h   GToolbar.h   GKeyboard.h   \
		GInputContext.h   GMultiTouch.h   GWebView.h
SOURCES = GSimpleLabel.cpp GProxyLabel.cpp GToolbar.cpp GKeyboard.cpp \
		GInputContext.cpp GMultiTouch.cpp GWebView.cpp  will_web.cpp

INCLUDEPATH += ../will_util

QMAKE_CXXFLAGS += $$(CXXFLAGS)
QMAKE_CFLAGS   += $$(CFLAGS)
QMAKE_LFLAGS   += $$(LDFLAGS)
