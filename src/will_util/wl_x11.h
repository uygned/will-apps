#ifndef WL_X11_H
#define WL_X11_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "wl_bitmap.h"

static Display *display = NULL;
static Visual *visual = NULL;

static int window_image_create(bitmap_t *canvas, XImage **image, int add_alpha,
		int color) {
	if (!canvas || !display || !visual)
		return 0;

	XImage *img = *image;
	if (img) {
		if (img->depth == canvas->depth && img->width == canvas->rect.width
				&& img->height == canvas->rect.height)
			return 1;
		XDestroyImage(img); // the data buffer is also detroyed
		img = NULL;
	}

#ifdef Z
//	int bytes_per_pixel = canvas->depth / 8;
	int bytes_per_pixel = canvas->depth == 8 ? 1 : 4;
	canvas->bytes_per_pixel = canvas->depth / 8;
	canvas->bytes_per_line = canvas->depth / 8 * canvas.width;
	int size = canvas->width * canvas->height * bytes_per_pixel;
#endif
	int bytes_per_pixel = canvas->depth / 8;
	canvas->bytes_per_pixel = bytes_per_pixel;
	if (canvas->depth >= 24) {
		bytes_per_pixel = 4;
		canvas->bytes_per_pixel = add_alpha ? 4 : 3;
	}
	canvas->bytes_per_line = canvas->rect.width * canvas->bytes_per_pixel;
	int size = canvas->rect.width * canvas->rect.height * bytes_per_pixel;
	canvas->data = (unsigned char *) malloc(size);
	if (!canvas->data) {
		perror("error: malloc()\n");
		return 0;
	}
	img = XCreateImage(display, visual, (unsigned int) canvas->depth, ZPixmap,
			0/*offset*/, (char *) canvas->data,
			(unsigned int) canvas->rect.width,
			(unsigned int) canvas->rect.height,
			bytes_per_pixel * 8/*bitmap_pad*/, 0);
	if (!img) {
		free(canvas->data);
		perror("error: XCreateImage()");
		return 0;
	}
	memset(canvas->data, color, size);
	img->bytes_per_line = canvas->bytes_per_line;
	img->bits_per_pixel = canvas->bytes_per_pixel * 8;
	printf(
			"canvas: size=%dx%d depth=%d bits_per_pixel=%d bitmap_pad=%d bitmap_unit=%d\n",
			img->width, img->height, img->depth, img->bits_per_pixel,
			img->bitmap_pad, img->bitmap_unit);
	*image = img;
	return 1;
}

static Window window;
static int screen;
static GC x11gc = NULL;

static int window_rotation = 0;
static bitmap_t bmp_output;
static bitmap_t bmp_canvas;
static XImage *img_output = NULL;
static XImage *img_canvas = NULL;

static void x11_free() {
	if (img_output) {
		XDestroyImage(img_output);
		img_output = NULL;
	}
	if (img_canvas) {
		XDestroyImage(img_canvas);
		img_canvas = NULL;
	}
	//XDestroyWindow(display, window);
	XCloseDisplay(display);
}

static int x11_init(int width, int height) {
	display = XOpenDisplay(getenv("DISPLAY")/*NULL*/);
	if (!display) {
		perror("XOpenDisplay");
		return 0;
	}

	screen = DefaultScreen(display);
	visual = DefaultVisual(display, screen);
	x11gc = DefaultGC(display, screen);

	bmp_output.depth = DefaultDepth(display, screen);
	bmp_output.rect.left = 0;
	bmp_output.rect.top = 0;
	bmp_output.rect.width = width > 0 ? width : DisplayWidth(display, screen);
	bmp_output.rect.height =
			height > 0 ? height : DisplayHeight(display, screen);
	bmp_output.bytes_per_line = 0;
	bmp_output.bytes_per_pixel = 0;
	bmp_output.data = NULL;

	bmp_canvas.depth = bmp_output.depth;
	bmp_canvas.rect.left = 0;
	bmp_canvas.rect.top = 0;
	bmp_canvas.rect.width = bmp_output.rect.height;
	bmp_canvas.rect.height = bmp_output.rect.width;
	bmp_canvas.bytes_per_line = 0;
	bmp_canvas.bytes_per_pixel = 0;
	bmp_canvas.data = NULL;

	window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
			bmp_output.rect.width, bmp_output.rect.height, 0/*border_width*/,
			BlackPixel(display, screen), WhitePixel(display, screen));
	printf("window: %dx%d depth=%d\n", bmp_output.rect.width,
			bmp_output.rect.height, bmp_output.depth);

	return 1;
}

typedef int (*x11_event_callback_t)(XEvent *event);
static int x11_quit = 0;
typedef int (*fd_read_callback_t)(int fd);

static void x11_show_window(long event_mask, x11_event_callback_t x11_callback,
		int fds[], fd_read_callback_t fd_callbacks[], int fd_count) {
	if (event_mask == 0) {
		event_mask = StructureNotifyMask | ResizeRedirectMask | ExposureMask
				| KeyPressMask | ButtonPress;
	}
	XSelectInput(display, window, event_mask);
	XMapWindow(display, window);
	XFlush(display);
	XEvent event;
	x11_quit = 0;

	if (fd_count < 1) {
		while (!x11_quit) {
			XNextEvent(display, &event);
			x11_callback(&event);
		}
		return;
	}

	// http://stackoverflow.com/questions/8592292/how-to-quit-the-blocking-of-xlibs-xnextevent
	int x11_fd = XConnectionNumber(display);
	if (x11_fd == -1) {
		printf("error: x11_fd == -1\n");
		return;
	}

	int i;
	int max_fd = x11_fd;
	for (i = 0; i < fd_count; i++) {
		if (fds[i] > max_fd)
			max_fd = fds[i];
	}
	max_fd++;

	fd_set in_fds;
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 1;
	int rc;
	while (!x11_quit) {
		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);
		for (i = 0; i < fd_count; i++)
			FD_SET(fds[i], &in_fds);

		rc = select(max_fd, &in_fds, NULL, NULL, &tv);
		if (rc == -1) {
			printf("error: select() returns -1\n");
			break;
		}

		if (rc == 0) // timeout
			continue;

		if (FD_ISSET(x11_fd, &in_fds)) {
			rc--;
			while (XPending(display)) {
				XNextEvent(display, &event);
				x11_callback(&event);
			}
		}
		if (rc) {
			for (i = 0; i < fd_count; i++) {
				if (FD_ISSET(fds[i], &in_fds)) {
					fd_callbacks[i](fds[i]);
					if (--rc == 0)
						break;
				}
			}
		}
	}
}

static void window_repaint(rect_s *rect) {
	if (window_rotation == 270)
		bitmap_copy(&bmp_output, &bmp_canvas, window_rotation);
	if (rect) {
		int x = rect->left;
		int y = rect->top;
		int w = rect->width;
		int h = rect->height;
		if (window_rotation == 270) {
			/*
			 *  +----------+ 	+-------------+
			 *  |   +----+ |    |   y         |
			 *  |   |    | |    | x >------+  |
			 *  |   |    | | <= |   |      |  |
			 *  | y ^----+ |    |   +------+  |
			 *  |   x      |    +-------------+
			 *  +----------+
			 */
			int t = x;
			x = y;
			y = bmp_canvas.rect.width - t;
			t = w;
			w = h;
			h = t;
			y -= h;
		}
//		printf("translated: %d,%d %dx%d -> %d,%d %dx%d\n", rect->left,
//				rect->top, rect->width, rect->height, x, y, w, h);
		XPutImage(display, window, x11gc, img_output, x, y, x, y,
				min(w, bmp_output.rect.width - x),
				min(h, bmp_output.rect.height - y));
	} else {
		XPutImage(display, window, x11gc, img_output, 0, 0, 0, 0,
				bmp_output.rect.width, bmp_output.rect.height);
	}
	XFlush(display);
}

static void x11_set_title(const char *title) {
	// http://en.literateprograms.org/Hello_World_(C,_Xlib)
	// http://neuron-ai.tuke.sk/hudecm/Tutorials/C/special/xlib-programming/xlib-programming-2.html
	XStoreName(display, window, title);
}

// https://www.opensource.apple.com/source/tcl/tcl-87/tcl_ext/tklib/tklib/modules/ctext/test.c?txt
static void send_map_notify() {
	XEvent event;
	event.type = MapNotify;
	event.xmap.type = MapNotify;
	event.xmap.window = window;
	event.xmap.display = display;
	event.xmap.event = window;
	XSendEvent(display, window, 0, StructureNotifyMask, &event);
	XFlush(display);
}

#endif
