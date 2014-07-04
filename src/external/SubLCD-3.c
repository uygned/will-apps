/* SubLCD-3,    by Kim �yhus,  kim@oyhus.no,   Copyright terms below.

FUNCTION:
   It doubles the horizontal resolution on LCD screens and similar,
   by considering the physical layout of LCD pixels,
   and how the eye interprets an LCD image.

   It reads a picture in the raw ppm/pnm P6 format,
   and outputs the same picture in half size
Typical use:
  ./SubLCD-3 < input_file_in_binary_raw.ppm > halved_output_file.ppm
Typical compiler command:
  gcc -o SubLCD-3 SubLCD-3.c  -lm -O3
*/


/*  GPL:
    SubLCD-3 Doubles resolution on LCDs, etc, while avoiding colour spatter.
    Copyright (C) 2008 Kim �yhus

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/



#include <stdio.h>
#include <math.h>     // For pow(,)
#include <string.h>   // Not much needs including.
#include <stdlib.h>   // for exit()
#include <limits.h>

#define MARGIN_X 4

#define x_y   ( (x)  +   (y)     *(Width+MARGIN_X) )
#define X_Y   ( (x)+(X)+((y)+(Y))*(Width+MARGIN_X) )

static float min = INT_MAX, max = INT_MIN;
static inline unsigned char get_byte(float x) {
	if (x < min)
		min = x;
	else if (x > max)
		max = x;
	int c = (int) (x * 255. + 0.5);
	return c < 0 ? 0 : (c <= 255 ? c : 255);
}

void SubLCD_a_picture(float (*fig)[3], float (*T)[3], float (*c)[3], float *o,
		int Width, int Height);

/* Perception levels are (in RGB order): 30, 59, 11) */

void sublcd_filter(unsigned char *dst, int dst_line_size, unsigned char *src,
	int Width, int Height) {

    float(*fig)[3];    // Dynamically sizable 2d array through double pointer.
    float(*T  )[3];    // Temporary
    float *o;          // Standard deviation
    float(*c  )[3];    // Smoothed colour

    /* Width and Height are increased to make margins
     * for exceeding indexes while doing the convolution
     */
    fig = malloc((Width+4) * (Height+2) *sizeof(float[3]));
    T   = malloc((Width+4) * (Height+2) *sizeof(float[3]));
    o   = malloc((Width+4) * (Height+2) *sizeof(float   ));
    c   = malloc((Width+4) * (Height+2) *sizeof(float[3]));

    fig += 2 + Width+MARGIN_X;  // Left and right margin of 2 pixels each.
    T   += 2 + Width+MARGIN_X;
    o   += 2 + Width+MARGIN_X;
    c   += 2 + Width+MARGIN_X;

	int x, y;
	for (y = -1; y < Height+1; y++) {
		if (MARGIN_X) {
			/* fill left and right margin */
			int margin[4] = {-2, -1, Width, Width + 1};
			int i;
			for (i = 0; i < 4; i++) {
				x = margin[i];
				fig[x_y][0] = 1.;
				fig[x_y][1] = 1.;
				fig[x_y][2] = 1.;
			}
		}
		/* fill top and bottom margin */
		if (y == -1 || y == Height) {
			for (x = 0; x < Width; x++) {
				fig[x_y][0] = 1.;
				fig[x_y][1] = 1.;
				fig[x_y][2] = 1.;
			}
			continue;
		}
		for (x = 0; x < Width; x++) {
			fig[x_y][0] = (0xff - *src++) / 255.;
			fig[x_y][1] = (0xff - *src++) / 255.;
			fig[x_y][2] = (0xff - *src++) / 255.;
		}
	}

    SubLCD_a_picture(fig, T, c, o, Width, Height);

//	for (y = -1; y < Height + 1; y++) {
//		for (x = -2; x < Width + 2; x++)
//			printf("%.1f/%.1f/%.1f ", fig[x_y][0], fig[x_y][0], fig[x_y][2]);
//		printf("\n");
//	}

	unsigned char *dst_line = dst, *d;
	for (y = 0; y < Height; y += 2) {
		d = dst_line;
		for (x = 0; x < Width; x += 2) {
			*d++ = 0xff - get_byte((fig[x_y][0] + fig[x_y + Width+MARGIN_X][0]) / 2.);
			*d++ = 0xff - get_byte((fig[x_y + 1][1] + fig[x_y + 1 + Width+MARGIN_X][1]) / 2.);
			*d++ = 0xff - get_byte((fig[x_y + 2][2] + fig[x_y + 2 + Width+MARGIN_X][2]) / 2.);
		}
		dst_line += dst_line_size;
	}
	printf("min max : %f %f\n", min, max);
}

void
smooth(float (*c)[3], float (*T)[3], float gauss[3][5], int Width, int Height)
{
    int x, y, X, Y, q;
    for (y = -1; y < Height+1; y++)  // Smoothing of SubLCD-1 for colour averaging.
	for (x = -2; x < Width +2; x++)
	{
	    c[x_y][0] = 0.;
	    c[x_y][1] = 0.;
	    c[x_y][2] = 0.;
	    for (Y = -1; Y <= 1; Y++)
	    {
	    	if ((y + Y) < -1 || (y + Y) >= (Height + 1))
	    		continue;
			for (X = -2; X <= 2; X++)
			{
				if ((x + X) < -2 || (x + X) >= (Width + 2))
					continue;
				for (q = 0; q < 3; q++)
					c[x_y][q] += T[X_Y][q] * gauss[Y+1][X+2];
			}
	    }
	}
}

/*
 1              1
 2            1   1
 4          1   2   1
 8        1   3   3   1
16      1   4   6   4   1
32    1   5  10  10   5   1
64  1  6   15  20  15   6   1
 */

void
SubLCD_a_picture(float (*fig)[3], float (*T)[3], float (*c)[3], float *o,
		int Width, int Height)
{
    int x, y, X, Y;

    static float gauss[3][5];
    static int virgin = 1;     // To run initialization once.
    if (virgin) {
    	virgin = 0;
		for (Y = 0; Y < 3; Y++)
		for (X = 0; X < 5; X++)
			gauss[Y][X] = (float[]) {1,4,6,4,1}[X] *
 					      (float[]) {1,2,1}[Y] / 64.;
    }

    for (y = -1; y < Height+1; y++)
	for (x = -2; x < Width +2; x++)  // SubLCD-1
	{
	    T[x_y][0] = fig[x_y][0] * 2.;
	    T[x_y][1] = 0.;
	    T[x_y][2] = fig[x_y][2] * 2.;
	    x++;
	    T[x_y][0] = 0.;
	    T[x_y][1] = fig[x_y][1] * 2.;
	    T[x_y][2] = 0.;
	}

	smooth(c, T, gauss, Width, Height);

	float s, t;

    /* Standard deviation */
	float x2, x_;
	for (y = -1; y < Height+1; y++)
	for (x = -2; x < Width +2; x++)
	{
		x2 = 0.;
		x_ = 0.;
		for (Y = -1; Y <= 1; Y++)
		{
			if ((y + Y) < -1 || (y + Y) >= (Height + 1))
				continue;
			for (X = -2; X <= 2; X++)
			{
				if ((x + X) < -2 || (x + X) >= (Width + 2))
					continue;
				t = ( .30 * fig[X_Y][0] +
					  .58 * fig[X_Y][1] +
					  .12 * fig[X_Y][2] );
				x2 += t*t*gauss[1+Y][2+X];
				x_ += t*  gauss[1+Y][2+X];
			}
		}
		o[x_y] = sqrtf(x2 - x_*x_);
		if ((x2 - x_*x_) < 0.) fprintf(stderr, "error sqrtf %f %f\n", x2, x_);
	}

    // probabilifying colour.  ( Marginalizing colour?)
	for (y = -1; y < Height+1; y++)
	for (x = -2; x < Width +2; x++)
	{
		T[x_y][0] = 0.;
		T[x_y][1] = 0.;
		T[x_y][2] = 0.;
		s = 0.;
		for (Y = -1; Y <= 1; Y++)
		{
			if ((y + Y) < -1 || (y + Y) >= (Height + 1))
				continue;
			for (X = -2; X <= 2; X++)
			{
				if ((x + X) < -2 || (x + X) >= (Width + 2))
					continue;
				if (o[X_Y] == 0) o[X_Y] = 0.000000000001;
				t  = gauss[Y+1][X+2] / o[X_Y];
				s += t;
				T[x_y][0] += c[X_Y][0] * t;
				T[x_y][1] += c[X_Y][1] * t;
				T[x_y][2] += c[X_Y][2] * t;
			}
		}
		if (s == 0) s = 0.000000000001;
		T[x_y][0] /= s;
		T[x_y][1] /= s;
		T[x_y][2] /= s;
	}

    for (y = -1; y < Height+1; y++)
	for (x = -2; x < Width +2; x++)
	{
	    float s;

	    T[x_y][0] -= fig[x_y][0];  // Difference between original and percieved.
	    T[x_y][1] -= fig[x_y][1];
	    T[x_y][2] -= fig[x_y][2];

	    s = .30 * T[x_y][0] +
			.58 * T[x_y][1] +
			.12 * T[x_y][2];   // Only colour difference.

	    T[x_y][0] -= s;
	    T[x_y][1] -= s;
	    T[x_y][2] -= s;
	}  // T[] now contains an image of how the eye sees the colour artifacts made by SubLCD-1.

    for (y = -1; y < Height + 1; y++) {
		for (x = -2; x < Width + 2; x++)
			printf("%.1f/%.1f/%.1f ", T[x_y][0], T[x_y][0], T[x_y][2]);
		printf("\n");
	}

    for (y = -1; y < Height+1; y++)
	for (x = -2; x < Width +2; x++)  // Copy input to output while compensating for visual artifacts.
	{
	    fig[x_y][0] -= 0.3 * (T[x_y-1][0] + 2.*T[x_y][0] + T[x_y+1][0]);
	    fig[x_y][1] -= 0.3 * (T[x_y-1][1] + 2.*T[x_y][1] + T[x_y+1][1]);
	    fig[x_y][2] -= 0.3 * (T[x_y-1][2] + 2.*T[x_y][2] + T[x_y+1][2]);
	}
}

void sublcd_filter2(unsigned char *dst, int dst_line_size, unsigned char *src,
		int width, int height) {
	unsigned char *dst_line = dst, *d;
	int x, y;
	for (y = 0; y < height; y += 2) {
		d = dst_line;
		for (x = 0; x < width; x += 2, src += 3) {
			*d++ = *src++;
			*d++ = *src++;
			*d++ = *src++;
		}
		dst_line += dst_line_size;
		src += width * 3;
	}
}

//#ifdef STDEV
//	for(y=-1; y<Height+1; y++)
//	for(x=-2; x<Width +2; x++)  // Writing a line of the image
//	{
//		x2 = 0;
//		for (Y = -1; Y <= 1; Y++)
//		{
//			if ((y + Y) < -1 || (y + Y) >= (Height + 1))
//				continue;
//			for (X = -2; X <= 2; X++)
//			{
//				if ((x + X) < -2 || (x + X) >= (Width + 2))
//					continue;
//				t = ( .30 * fig[X_Y][0] +
//					  .58 * fig[X_Y][1] +
//					  .12 * fig[X_Y][2] );
//				x2 += (t-o[x_y])*(t-o[x_y])*gauss[1+Y][2+X];
//			}
//		}
//		o[x_y] = sqrtf(x2);
//		if (x2 < 0.) fprintf(stderr, "error sqrt %f\n", x2);
//	}
//#endif

//	    fig[x_y][0] += T[x_y][0];
//	    fig[x_y][1] += T[x_y][1];
//	    fig[x_y][2] += T[x_y][2];
//	    fig[x_y][0] += 0.3*( T[x_y-1][0] +2.*T[x_y][0] + T[x_y+1][0] );
//	    fig[x_y][1] += 0.3*( T[x_y-1][1] +2.*T[x_y][1] + T[x_y+1][1] );
//	    fig[x_y][2] += 0.3*( T[x_y-1][2] +2.*T[x_y][2] + T[x_y+1][2] );
//	    fig[x_y][0] = T[x_y][0];
//	    fig[x_y][1] = T[x_y][1];
//	    fig[x_y][2] = T[x_y][2];
