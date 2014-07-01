/* SubLCD-3,    by Kim Øyhus,  kim@oyhus.no,   Copyright terms below.

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
    Copyright (C) 2008 Kim Øyhus

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

#define WIDTH 10000    // Maximum width of picture.
#define x_y   ( x  + y   *Width )
#define X_Y   ( x+X+(y+Y)*Width )


/* READING PICTURE
This function just reads the picture, line by line,
from standard in, after first analyzing it.

The picture line is in a global variable to keep things simple.
Object oriented obfuscators believe this to be wrong.
 */


int   Width, Height;  // The real width and height of the input picture.
                   //  Output values are half of this.
float line    [WIDTH][3];
float line_out[WIDTH][3];   // The resulting line, obviously


// Input and output images.
float(*fig)[4];    // Dynamically sizable 2d array through double pointer.
float(*T  )[4];    // Temporary
float *o;          // Standard deviation
float(*c  )[4];    // Smoothed colour


               void
Read_a_picture()
{
    char s[WIDTH+2];  // +2 in case of CR & LF

/* For a PNM P6 file to be readable here, it should have the format:
   P6            F.ex:   P6
   width height          320 240    Some have an unsupported lineshift in there.
   colours               256
   raw data              Yujbdfn2#A...
*/
    fgets(s, 100, stdin);
    if(strncmp("P6",s,2) !=0)
	fprintf(stderr, "This is not a PNM, P6 file.\n");
    
    do  // Removal of comments in image file
	fgets(s, 1000, stdin);
    while(s[0]=='#');
    
    sscanf(s, "%d %d", &Width, &Height);
    fprintf(stderr, "Width=%d Height=%d\n", Width, Height);
    fgets(s, 100, stdin);  // Removal of last line
    
    
    /* Allocating memory for the images */
    /* Width and Height are increased to make margins
     * for exceeding indexes while doing the convolution
     */
    fig = malloc( (Width+4) * (Height+2) *sizeof(float[4]));
    T   = malloc( (Width+4) * (Height+2) *sizeof(float[4]));
    o   = malloc( (Width+4) * (Height+2) *sizeof(float   ));
    c   = malloc( (Width+4) * (Height+2) *sizeof(float[4]));

    fig += 2 + Width;  // Left and right margin of 2 pixels each.
    T   += 2 + Width;
    o   += 2 + Width;
    c   += 2 + Width;


    {
	int x, y;  // Clearing of all arrays, just to be sure they are initialized. Edge effects.
	for(    y=-1; y<Height+1; y++)  
	    for(x=-2; x<Width +2; x++)
	    {
		fig[x_y][0] =fig[x_y][1] =fig[x_y][2] = 0.;
		T[x_y][0] =  T[x_y][1] =  T[x_y][2] = 0.;
		o[x_y]                              = 0.;
		c[x_y][0] =  c[x_y][1] =  c[x_y][2] = 0.;
	    }
    }


// Reading from image file

    int x, y;
    for(    y=0; y<Height; y++)
	for(x=0; x<Width ; x++)  // Scaling to [0,1> and inverse gamma correction.
	{
	    fig[x_y][0] = powf( fgetc(stdin) /255., 2.2);
	    fig[x_y][1] = powf( fgetc(stdin) /255., 2.2);
	    fig[x_y][2] = powf( fgetc(stdin) /255., 2.2);
	}
}



             void
Write_a_char(float x)
{
    if( x<0.)	x=0.;
    if( x>1.)	x=1.;
    putchar( (int) (powf(x, 1./2.2)*255.+0.5) );  // Gamma correction and scaling.
}


                void
Write_a_picture()
{
    int x, y;

    if(0)
    {
	printf("P6\n%d %d\n255\n", Width/1, Height/1);  // Header of image file
	for(    y=0; y<Height; y++)
	    for(x=0; x<Width ; x++)  // Writing a line of the image
	    {
		Write_a_char(fig[x_y][0]);
		Write_a_char(fig[x_y][1]);
		Write_a_char(fig[x_y][2]);
	    }
    }
    
    
    if(1)
    {
	printf("P6\n%d %d\n255\n", Width/2, Height/2);  // Header of image file
	for(y=0;     y<Height; y+=2)
	    for(x=0; x<Width ; x+=2)  // Writing a line of the image
	    {
		Write_a_char(( fig[x_y  ][0] + fig[x_y  +Width][0] )/2.);
		Write_a_char(( fig[x_y+1][1] + fig[x_y+1+Width][1] )/2.);
		Write_a_char(( fig[x_y+2][2] + fig[x_y+2+Width][2] )/2.);
	    }
    }
}




       void
smooth(float (*c)[4],
       float (*T)[4])
{
    int x, y, X, Y, q;
    
    for(    y=0; y<Height; y++)  // Smoothing of SubLCD-1 for colour averaging.
	for(x=0; x<Width ; x++)
	{
	    c[x_y][0] = 0.;
	    c[x_y][1] = 0.;
	    c[x_y][2] = 0.;
	    
	    for(        Y=-1; Y<=1; Y++)
		if(y>0)
		    for(X=-2; X<=2; X++)
			for(q=0; q<4; q++)
			    c[x_y][q] += T[X_Y][q] *
				(float[]){1,4,6,4,1}[X+2] *
				(float[]){1,2,1}    [Y+1] / 64.;
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
SubLCD_a_picture()
{
    int   x, y, X, Y;
    float gauss[3][5];

    static int 
	virgin = 1;     // To run initialization once.
    if( virgin )
    {   virgin = 0;   //   Not virgin any more.

	for(    Y=0; Y<3; Y++)
	    for(X=0; X<5; X++)
		gauss[Y][X] = (float[]) {1,4,6,4,1}[X] *
			      (float[])     {1,2,1}[Y] / 64.;
    }


    for(    y=0; y<Height; y++)
	for(x=0; x<Width ; x++)  // SubLCD-1
	{
	    T[x_y][0] = fig[x_y][0] *2.;
	    T[x_y][1] = 0.             ;
	    T[x_y][2] = fig[x_y][2] *2.;
	    x++;
	    T[x_y][0] = 0.             ;
	    T[x_y][1] = fig[x_y][1] *2.;
	    T[x_y][2] = 0.             ;
	}

    smooth( c, T);

  
/* Standard deviation */
    {
	float x2, x_, t;  // x² and average of x, for calculation of standard deviation.
	for(    y=0; y<Height; y++)
	    for(x=0; x<Width ; x++)  // Writing a line of the image
	    {
		x2 = 0.0001;  // This small, but not too small, number avoids some error...
		x_ = 0.;
		for(        Y=-1; Y<=1; Y++)
		    if(y>0)
			for(X=-2; X<=2; X++)
			{
			    t = ( .30 * fig[X_Y][0] +
				  .58 * fig[X_Y][1] +
				  .12 * fig[X_Y][2] );
			    x2 += t*t*gauss[1+Y][2+X];
			    x_ += t*  gauss[1+Y][2+X];
			}
		o[ x_y ] = sqrtf(x2 - x_*x_);
		//if( (x2 - x_*x_)<=0.) fprintf( stderr, "ASDF  %f  %f\n", x2, x_);
	    } 
    }


// probabilifying colour.  ( Marginalizing colour?)
    {
	float s, t;

	for(    y=0; y<Height; y++)  
	    for(x=0; x<Width ; x++)
	    {
		T[x_y][0] = 0.;
		T[x_y][1] = 0.;
		T[x_y][2] = 0.;
		s          = 0.000000000001;
		for(        Y=-1; Y<=1; Y++)
		    if(y>0)
			for(X=-2; X<=2; X++)
			{
			    t  = gauss[Y+1][X+2] / o[ X_Y ] ;
			    s += t                             ;
			    T[x_y][0] += c[X_Y][0] * t   ;
			    T[x_y][1] += c[X_Y][1] * t   ;
			    T[x_y][2] += c[X_Y][2] * t   ;
			}
		T[x_y][0] /= s;
		T[x_y][1] /= s;
		T[x_y][2] /= s;
	    }
    }
	

    for(    y=0; y<Height; y++)  
	for(x=0; x<Width ; x++)
	{
	    float s;
	    
	    T[x_y][0] -= fig[x_y][0];  // Difference between original and percieved.
	    T[x_y][1] -= fig[x_y][1];
	    T[x_y][2] -= fig[x_y][2];
	    
	    s = .30* T[x_y][0] +
		.58* T[x_y][1] +
		.12* T[x_y][2];   // Only colour difference.
	    T[x_y][0] -= s;
	    T[x_y][1] -= s;
	    T[x_y][2] -= s;
	}  // T[] now contains an image of how the eye sees the colour artifacts made by SubLCD-1.
    
    
    for(    y=0; y<Height; y++)
	for(x=0; x<Width ; x++)  // Copy input to output while compensating for visual artifacts.
	{
	    fig[x_y][0] -= 0.3*( T[x_y-1][0] +2.*T[x_y][0] + T[x_y+1][0] );
	    fig[x_y][1] -= 0.3*( T[x_y-1][1] +2.*T[x_y][1] + T[x_y+1][1] );
	    fig[x_y][2] -= 0.3*( T[x_y-1][2] +2.*T[x_y][2] + T[x_y+1][2] );
	}


}




     int
main()
{
    Read_a_picture();
    SubLCD_a_picture();  // Looks pretty with 2 iterations, but it is mathematically wrong,
    SubLCD_a_picture(); //  because the original picture gets modified.
    Write_a_picture();
}
