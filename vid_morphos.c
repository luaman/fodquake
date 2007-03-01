/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"
#include "in_morphos.h"

extern viddef_t vid;

struct display
{
	void *inputdata;

	char *buffer;
	short *zbuffer;
	char *sbuffer;

	struct Screen *screen;
	struct Window *window;

	struct ScreenBuffer *screenbuffers[3];
	int currentbuffer;

	void *pointermem;

	struct RastPort rastport;

	char pal[256 * 4];
};

cvar_t _windowed_mouse = { "_windowed_mouse", "1", CVAR_ARCHIVE };

void Sys_Video_CvarInit(void)
{
}

void *Sys_Video_Open(int width, int height, int depth, int fullscreen, unsigned char *palette)
{
	struct display *d;
	int i;

	Cvar_Register(&_windowed_mouse);

	d = AllocVec(sizeof(*d), MEMF_CLEAR);
	if (d)
	{
		vid.width = width;
		vid.height = height;
		vid.maxwarpwidth = WARP_WIDTH;
		vid.maxwarpheight = WARP_HEIGHT;
		vid.colormap = host_colormap;

		d->buffer = AllocVec(vid.width * vid.height, MEMF_ANY);
		if (d->buffer == 0)
		{
			Sys_Error("VID: Couldn't allocate frame buffer");
		}

		d->zbuffer = AllocVec(vid.width * vid.height * sizeof(*d_pzbuffer), MEMF_ANY);
		if (d->zbuffer == 0)
		{
			Sys_Error("VID: Couldn't allocate zbuffer");
		}

		d->sbuffer = AllocVec(D_SurfaceCacheForRes(vid.width, vid.height), MEMF_ANY);
		if (d->sbuffer == 0)
		{
			Sys_Error("VID: Couldn't allocate surface cache");
		}

		D_InitCaches(d->sbuffer, D_SurfaceCacheForRes(vid.width, vid.height));

		d_pzbuffer = d->zbuffer;

		vid.rowbytes = vid.width;
		vid.buffer = d->buffer;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
		vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

		if (fullscreen)
		{
			d->screen = OpenScreenTags(0,
				SA_Width, vid.width,
				SA_Height, vid.height,
				SA_Depth, 8,
				SA_Quiet, TRUE,
				TAG_DONE);
		}


		d->window = OpenWindowTags(0,
			WA_InnerWidth, vid.width,
			WA_InnerHeight, vid.height,
			WA_Title, "FodQuake",
			WA_DragBar, d->screen ? FALSE : TRUE,
			WA_DepthGadget, d->screen ? FALSE : TRUE,
			WA_Borderless, d->screen ? TRUE : FALSE,
			WA_RMBTrap, TRUE, d->screen ?
			WA_PubScreen : TAG_IGNORE, (ULONG) d->screen,
			WA_Activate, TRUE,
			WA_ReportMouse, TRUE,
			TAG_DONE);

		if (d->window == 0)
			Sys_Error("Unable to open window");

		d->pointermem = AllocVec(256, MEMF_ANY | MEMF_CLEAR);
		if (d->pointermem == 0)
		{
			Sys_Error("Unable to allocate memory for mouse pointer");
		}

		SetPointer(d->window, d->pointermem, 16, 16, 0, 0);

		vid.numpages = d->screen ? 3 : 1;

		if (d->screen)
		{
			for (i = 0; i < 3; i++)
			{
				d->screenbuffers[i] = AllocScreenBuffer(d->screen, 0, i ? SB_COPY_BITMAP : SB_SCREEN_BITMAP);
				if (d->screenbuffers[i] == 0)
				{
					Sys_Error("Unable to set up triplebuffering");
				}
			}
		}

		d->currentbuffer = 0;

		InitRastPort(&d->rastport);

		Sys_Video_SetPalette(d, palette);

		d->inputdata = Sys_Input_Init(d->screen, d->window);
	}

	return d;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;
	int i;

	D_FlushCaches();

	if (d->inputdata)
		Sys_Input_Shutdown(d->inputdata);

	if (d->buffer)
		FreeVec(d->buffer);

	if (d->zbuffer)
		FreeVec(d->zbuffer);

	if (d->sbuffer)
		FreeVec(d->sbuffer);

	for (i = 0; i < 3; i++)
	{
		if (d->screenbuffers[i])
			FreeScreenBuffer(d->screen, d->screenbuffers[i]);
	}

	if (d->window)
		CloseWindow(d->window);

	if (d->pointermem)
		FreeVec(d->pointermem);

	if (d->screen)
		CloseScreen(d->screen);

	FreeVec(d);
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

void Sys_Video_Update(void *display, vrect_t * rects)
{
	struct display *d = display;

	while (rects)
	{
		if (d->screen)
		{
			d->rastport.BitMap = d->screenbuffers[d->currentbuffer]->sb_BitMap;
			WritePixelArray(d->buffer, rects->x, rects->y, vid.rowbytes, &d->rastport, rects->x, rects->y, rects->width, rects->height, RECTFMT_LUT8);
		}
		else
		{
			WriteLUTPixelArray(d->buffer, rects->x, rects->y, vid.rowbytes, d->window->RPort, d->pal, d->window->BorderLeft + rects->x, d->window->BorderTop + rects->y, rects->width, rects->height, CTABFMT_XRGB8);
		}


		rects = rects->pnext;
	}

	if (d->screen)
	{
		ChangeScreenBuffer(d->screen, d->screenbuffers[d->currentbuffer]);
		d->currentbuffer++;
		d->currentbuffer %= 3;
	}
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	if (dograb && !d->screen)
	{
		if (dograb)
		{
			/* Hide pointer */

			SetPointer(d->window, d->pointermem, 16, 16, 0, 0);
		}
		else
		{
			/* Show pointer */

			ClearPointer(d->window);
		}
	}
}

void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
	struct display *d = display;
	int i;

	ULONG spal[1 + (256 * 3) + 1];

	if (d->screen)
	{
		spal[0] = 256 << 16;

		for (i = 0; i < 256; i++)
		{
			spal[1 + (i * 3)] = ((unsigned int)palette[i * 3]) << 24;
			spal[2 + (i * 3)] = ((unsigned int)palette[i * 3 + 1]) << 24;
			spal[3 + (i * 3)] = ((unsigned int)palette[i * 3 + 2]) << 24;
		}

		spal[1 + (3 * 256)] = 0;

		LoadRGB32(&d->screen->ViewPort, spal);
	}

	for (i = 0; i < 256; i++)
	{
		d->pal[i * 4] = 0;
		d->pal[i * 4 + 1] = palette[i * 3 + 0];
		d->pal[i * 4 + 2] = palette[i * 3 + 1];
		d->pal[i * 4 + 3] = palette[i * 3 + 2];
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
	struct display *d;

	d = display;

	SetWindowTitles(d->window, text, (void *)-1);
}

