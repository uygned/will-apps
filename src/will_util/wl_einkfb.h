#ifndef WL_EINKFB_H
#define WL_EINKFB_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

typedef unsigned char u8;
#include <linux/fb.h>
#include <linux/einkfb.h>
#include <linux/mxcfb.h>
#include <sys/mman.h>

static int einkfb_fd = -1;
static unsigned char *einkfb_data = NULL;
static int einkfb_mem_len = 0;
static int einkfb_mem_offset = 0;
//static struct update_area_t einkfb_area;
//static struct mxcfb_update_data mxcfb_data;
//static struct fb_fix_screeninfo finfo;
//static struct fb_var_screeninfo vinfo;
static int einkfb_left = 0;
static int einkfb_top = 0;
static int einkfb_width = 0;
static int einkfb_height = 0;
static int einkfb_pixel_bits = 0;
static int einkfb_line_size = 0;

#ifdef X
static void dump_einkfb()
{
	FILE *file = fopen("einkfb_dump", "w");
	int n;
	for (n = 0; n < einkfb_mem_len / 4; n++) {
		if (n % finfo.line_length == 0)
		fprintf(file, "\n");
		fprintf(file, "%02x", einkfb_data[n]);
	}
	fclose(file);
}
#endif

static int einkfb_init() {
	einkfb_fd = open(EINK_FRAME_BUFFER, O_RDWR);
//	if (einkfb_fd == -1)
//		einkfb_fd = open(EINK_FRAME_BUFFER_ALT_NAME, O_RDWR);
	if (einkfb_fd == -1) {
		perror("einkfb_fd");
		return 0;
	}

	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	memset(&vinfo, 0, sizeof(vinfo));
	memset(&finfo, 0, sizeof(finfo));

	if (ioctl(einkfb_fd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("FBIOGET_FSCREENINFO");
		close(einkfb_fd);
		einkfb_fd = -1;
		return 0;
	}
	if (ioctl(einkfb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("FBIOGET_VSCREENINFO");
		close(einkfb_fd);
		einkfb_fd = -1;
		return 0;
	}

	printf("[EINKFB] %s: size=%dx%d(%dx%d/%dx%d) offset=%d,%d pixel=%d gray=%d "
			"line_len=%d mem_len=%d type=%d visual=%d accel=%d\n", finfo.id,
			vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual,
			vinfo.width, vinfo.height, vinfo.xoffset, vinfo.yoffset,
			vinfo.bits_per_pixel, vinfo.grayscale, finfo.line_length,
			finfo.smem_len, finfo.type, finfo.visual, finfo.accel);
	// FB_TYPE_PACKED_PIXELS FB_VISUAL_STATIC_PSEUDOCOLOR

	einkfb_data = (unsigned char *) mmap(NULL, finfo.smem_len,
	PROT_READ | PROT_WRITE, MAP_SHARED, einkfb_fd, 0);
	if (einkfb_data == MAP_FAILED) {
		perror("einkfb mmap()");
		close(einkfb_fd);
		einkfb_fd = -1;
		einkfb_data = NULL;
		return 0;
	}

	einkfb_mem_len = finfo.smem_len;
	einkfb_mem_offset = vinfo.yoffset * finfo.line_length
			+ vinfo.xoffset * vinfo.bits_per_pixel / 8;
	einkfb_data += einkfb_mem_offset;

	einkfb_width = vinfo.xres - vinfo.xoffset;
	einkfb_height = vinfo.yres - vinfo.yoffset;
	einkfb_left = vinfo.xoffset;
	einkfb_top = vinfo.yoffset;
	einkfb_pixel_bits = vinfo.bits_per_pixel;
	einkfb_line_size = finfo.line_length;

	return 1;
}

static void einkfb_free() {
	if (einkfb_data) {
		munmap(einkfb_data - einkfb_mem_offset, einkfb_mem_len);
		einkfb_data = NULL;
	}
	if (einkfb_fd != -1) {
		close(einkfb_fd);
		einkfb_fd = -1;
	}
}

static void einkfb_update_mxcfb(int x, int y, int w, int h) {
	if (w <= 0 || h <= 0)
		return;

	struct mxcfb_update_data mxcfb_data;
	mxcfb_data.waveform_mode = WAVEFORM_MODE_GC16_FAST;
	mxcfb_data.update_mode = UPDATE_MODE_PARTIAL;
	mxcfb_data.temp = TEMP_USE_AUTO;
	mxcfb_data.flags = 0;

	mxcfb_data.update_region.left = x;
	mxcfb_data.update_region.top = y;
	mxcfb_data.update_region.width = w;
	mxcfb_data.update_region.height = h;
	ioctl(einkfb_fd, MXCFB_SEND_UPDATE, &mxcfb_data);
}

//ioctl(d_ptr->fd, FBIOBLANK, on ? VESA_POWERDOWN : VESA_NO_BLANKING);

/*
 static void einkfb_update(int x0, int y0, int x1, int y1) {
 struct update_area_t einkfb_area;
 einkfb_area.x1 = x0;
 einkfb_area.y1 = y0;
 einkfb_area.x2 = x1 + 1;
 einkfb_area.y2 = y1 + 1;
 einkfb_area.which_fx = fx_update_partial; // fx_update_partial fx_update_full fx_invert
 einkfb_area.buffer = NULL;
 ioctl(einkfb_fd, FBIO_EINK_UPDATE_DISPLAY_AREA, &einkfb_area);
 }

 static void draw_line(int y, unsigned char color) {
 unsigned char *line = einkfb_buf + fb_line_size * y;
 __u32 i;
 for (i = 0; i < fb_line_size; i++) {
 line[i] = color;
 }
 }
 */

//#define FBSIZE (600/8*800)
////==================================
//// gmplay4 - play video on 4-bit fb0
////---------------------------------
//void gmplay4(void) {
//    u32 i,x,y,b,p,off=(MY/2-400)*fs+MX/4-150,fbsize=FBSIZE;
//    u8 fbt[FBSIZE];
//    while (fread(fbt,fbsize,1,stdin)) {
//        teu+=130; // teu: next update time
//        if (getmsec()>teu+1000) continue; // drop frame if > 1 sec behind
//        gmlib(GMLIB_VSYNC); // wait for fb0 ready
//        for (y=0; y<800; y++) for (x=0; x<600; x+=8) {
//                b=fbt[600/8*y+x/8];
//                i=y*fs+x/2+off;
//                p=(b&1)*240;
//                b>>=1;
//                fb0[i]=p|(b&1)*15;
//                b>>=1;
//                p=(b&1)*240;
//                b>>=1;
//                fb0[i+1]=p|(b&1)*15;
//                b>>=1;
//                p=(b&1)*240;
//                b>>=1;
//                fb0[i+2]=p|(b&1)*15;
//                b>>=1;
//                p=(b&1)*240;
//                b>>=1;
//                fb0[i+3]=p|(b&1)*15;
//            }
//        fc++;
//        gmlib(GMLIB_UPDATE);
//    }
//}
////==================================
//// gmplay8 - play video on 8-bit fb0
////----------------------------------
//void gmplay8(void) {
//    u32 i,x,y,b,fbsize=FBSIZE;
//    u8 fbt[FBSIZE];
//    while (fread(fbt,fbsize,1,stdin)) {
//        teu+=130; // teu: next update time
//        if (getmsec()>teu+1000) continue; // drop frame if > 1 sec behind
//        gmlib(GMLIB_VSYNC); // wait for fb0 ready
//        for (y=0; y<800; y++)
//        	for (x=0; x<600; x+=8) {
//                b=fbt[600/8*y+x/8];
//                i=y*fs+x;
//                fb0[i]=(b&1)*255;
//                b>>=1;
//                fb0[i+1]=(b&1)*255;
//                b>>=1;
//                fb0[i+2]=(b&1)*255;
//                b>>=1;
//                fb0[i+3]=(b&1)*255;
//                b>>=1;
//                fb0[i+4]=(b&1)*255;
//                b>>=1;
//                fb0[i+5]=(b&1)*255;
//                b>>=1;
//                fb0[i+6]=(b&1)*255;
//                b>>=1;
//                fb0[i+7]=(b&1)*255;
//            }
//        fc++;
//        gmlib(GMLIB_UPDATE);
//    }
//}
#endif
