/*
Copyright (C) 2008 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>

#include <stdlib.h>
#include <string.h>

#define false qfalse
#define true qtrue
#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"
#include "in_wii.h"
#undef false
#undef true

struct display
{
	unsigned int width;
	unsigned int height;

	void *inputdata;

	unsigned char *buffer8;
	unsigned short *buffer;

	unsigned char palr[256];
	unsigned char palg[256];
	unsigned char palb[256];
};

void Sys_Video_CvarInit(void)
{
}

void *Sys_Video_Open(unsigned int width, unsigned int height, unsigned int depth, int fullscreen, unsigned char *palette)
{
	struct display *d;

	d = malloc(sizeof(*d));
	if (d)
	{
		GXRModeObj *rmode;

		switch(VIDEO_GetCurrentTvMode())
		{
			case VI_NTSC:
				rmode = &TVNtsc480IntDf;
				break;

			case VI_PAL:
				rmode = &TVPal528IntDf;
				break;

			case VI_MPAL:
				rmode = &TVMpal480IntDf;
				break;

			default:
				rmode = &TVNtsc480IntDf;
				break;
		}

		d->width = rmode->fbWidth;
		d->height = rmode->xfbHeight;

		d->buffer8 = malloc(d->width * d->height);
		if (d->buffer8)
		{
			memset(d->buffer8, 0, d->width * d->height);

			d->buffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
			if (d->buffer)
			{
				memset(d->buffer, 0, d->width * d->height * 2);
				#if 1
				VIDEO_Configure(rmode);
				VIDEO_SetNextFramebuffer(d->buffer);
				VIDEO_SetBlack(FALSE);
				VIDEO_Flush();
				VIDEO_WaitVSync();
				#endif

				d->inputdata = Sys_Input_Init();
				if (d->inputdata)
				{
					return d;
				}

#warning Free fb
			}

			free(d->buffer8);
		}

		free(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d;

	d = display;

#warning Free fb
	free(d->buffer8);
	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 1;
}

void Sys_Video_GetEvents(void *display)
{
	struct display *d = display;

	Sys_Input_GetEvents(d->inputdata);
}
 
void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

	Sys_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

unsigned int CvtRGB (unsigned char r1, unsigned char g1, unsigned char b1, unsigned char r2, unsigned char g2, unsigned char b2)
{
  int y1, cb1, cr1, y2, cb2, cr2, cb, cr;

  y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
  cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
  cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

  y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
  cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
  cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

  cb = (cb1 + cb2) >> 1;
  cr = (cr1 + cr2) >> 1;

  return (y1 << 24) | (cb << 16) | (y2 << 8) | cr;
}

void Sys_Video_Update(void *display, vrect_t * rects)
{
	struct display *d = display;
	unsigned int *dest;
	unsigned char *source;

	unsigned int startx;
	unsigned int width;
	unsigned int x;
	unsigned int y;

#if 1
	while (rects)
	{
		startx = rects->x&~1;
		width = (rects->width + (rects->x - startx) + 1)&~1;
		source = d->buffer8 + rects->y * d->width + startx;
		dest = (unsigned int *)(d->buffer + rects->y * d->width + startx);

		for(y=0;y<rects->height;y++)
		{
			for(x=0;x<width;x+=2)
			{
				dest[x/2] = CvtRGB(d->palr[source[x]], d->palg[source[x]], d->palb[source[x]], d->palr[source[x+1]], d->palg[source[x+1]], d->palb[source[x+1]]);
			}

			source+= d->width;
			dest+= d->width/2;
		}

		rects = rects->pnext;
	}
#endif
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
}

void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
	struct display *d = display;
	int i;

	for (i = 0; i < 256; i++)
	{
		d->palr[i] = palette[i * 3 + 0];
		d->palg[i] = palette[i * 3 + 1];
		d->palb[i] = palette[i * 3 + 2];
	}
}

void Sys_SendKeyEvents()
{
}

void VID_LockBuffer()
{
}

void VID_UnlockBuffer()
{
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
}

unsigned int Sys_Video_GetWidth(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	struct display *d;

	d = display;

	return d->height;
}

unsigned int Sys_Video_GetBytesPerRow(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

void *Sys_Video_GetBuffer(void *display)
{
	struct display *d;

	d = display;

	return d->buffer8;
}

