/*
Copyright (C) 1996-1997 Id Software, Inc.
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
// vid_x11.c -- general x video driver

#define _BSD

typedef unsigned short PIXEL16;
typedef unsigned int PIXEL24;

#ifndef __CYGWIN__
#define USE_VMODE 1
#endif

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#if USE_VMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"
#include "in_x11.h"

struct display
{
	void *inputdata;
#if USE_VMODE
	XF86VidModeModeInfo **vidmodes;
	XF86VidModeModeInfo origvidmode;
	qboolean vidmode_active;
#endif
	int scrnum;
	qboolean doShm; 
	Display *x_disp;
	Colormap x_cmap;
	Window x_win;
	GC x_gc;
	Visual *x_vis;
	XVisualInfo *x_visinfo;
	int current_framebuffer;
	XImage *x_framebuffer[2];
	XShmSegmentInfo x_shminfo[2];
	int x_shmeventtype;

	int width;
	int height;
	int depth;
	int fullscreen;

	byte current_palette[768];

	PIXEL16 st2d_8to16table[256];
	PIXEL24 st2d_8to24table[256];
	long r_shift, g_shift, b_shift;
	unsigned long r_mask, g_mask, b_mask;
};

static void shiftmask_init(struct display *d)
{
	unsigned int x;
	d->r_mask = d->x_vis->red_mask;
	d->g_mask = d->x_vis->green_mask;
	d->b_mask = d->x_vis->blue_mask;

	if (d->r_mask > (1 << 31) || d->g_mask > (1 << 31) || d->b_mask > (1 << 31))
		Sys_Error("XGetVisualInfo returned bogus rgb mask");

	for (d->r_shift = -8, x = 1; x < d->r_mask; x = x << 1)
		d->r_shift++;
	for (d->g_shift = -8, x = 1; x < d->g_mask; x = x << 1)
		d->g_shift++;
	for (d->b_shift = -8, x = 1; x < d->b_mask; x = x << 1)
		d->b_shift++;
}

static PIXEL16 xlib_rgb16(struct display *d, int r, int g, int b)
{
	PIXEL16 p;

	p = 0;

	if (d->r_shift > 0)
	{
		p = (r << (d->r_shift)) & d->r_mask;
	}
	else if (d->r_shift < 0)
	{
		p = (r >> (-d->r_shift)) & d->r_mask;
	}
	else
	{
		p |= (r & d->r_mask);
	}

	if (d->g_shift > 0)
	{
		p |= (g << (d->g_shift)) & d->g_mask;
	}
	else if (d->g_shift < 0)
	{
		p |= (g >> (-d->g_shift)) & d->g_mask;
	}
	else
	{
		p |= (g & d->g_mask);
	}

	if (d->b_shift > 0)
	{
		p |= (b << (d->b_shift)) & d->b_mask;
	}
	else if (d->b_shift < 0)
	{
		p |= (b >> (-d->b_shift)) & d->b_mask;
	}
	else
	{
		p |= (b & d->b_mask);
	}

	return p;
}

static PIXEL24 xlib_rgb24(struct display *d, int r, int g, int b)
{
	PIXEL24 p;

	p = 0;

	if (d->r_shift > 0)
	{
		p = (r << (d->r_shift)) & d->r_mask;
	}
	else if (d->r_shift < 0)
	{
		p = (r >> (-d->r_shift)) & d->r_mask;
	}
	else
	{
		p |= (r & d->r_mask);
	}

	if (d->g_shift > 0)
	{
		p |= (g << (d->g_shift)) & d->g_mask;
	}
	else if (d->g_shift < 0)
	{
		p |= (g >> (-d->g_shift)) & d->g_mask;
	}
	else
	{
		p |= (g & d->g_mask);
	}

	if (d->b_shift > 0)
	{
		p |= (b << (d->b_shift)) & d->b_mask;
	}
	else if (d->b_shift < 0)
	{
		p |= (b >> (-d->b_shift)) & d->b_mask;
	}
	else
	{
		p |= (b & d->b_mask);
	}

	return p;
}

static void st2_fixup(struct display *d, XImage * framebuf, int x, int y, int width, int height)
{
	int xi, yi;
	unsigned char *src;
	PIXEL16 *dest;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < y + height; yi++)
	{
		src = &framebuf->data[yi * framebuf->bytes_per_line];
		dest = (PIXEL16 *) src;
		for (xi = (x + width - 1); xi >= x; xi--)
		{
			dest[xi] = d->st2d_8to16table[src[xi]];
		}
	}
}

static void st3_fixup(struct display *d, XImage * framebuf, int x, int y, int width, int height)
{
	int xi, yi;
	unsigned char *src;
	PIXEL24 *dest;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < y + height; yi++)
	{
		src = &framebuf->data[yi * framebuf->bytes_per_line];
		dest = (PIXEL24 *) src;
		for (xi = (x + width - 1); xi >= x; xi--)
		{
			dest[xi] = d->st2d_8to24table[src[xi]];
		}
	}
}

// ========================================================================
// makes a null cursor
// ========================================================================

static Cursor CreateNullCursor(Display *display, Window root)
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(display, root, 1, 1, 1 /*depth */ );
	xgc.function = GXclear;
	gc = XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask, &dummycolour, &dummycolour, 0, 0);
	XFreePixmap(display, cursormask);
	XFreeGC(display, gc);

	return cursor;
}

static void ResetFrameBuffer(struct display *d)
{
	int mem, pwidth;

	if (d->x_framebuffer[0])
	{
		free(d->x_framebuffer[0]->data);
		free(d->x_framebuffer[0]);
	}

	pwidth = d->x_visinfo->depth / 8;
	if (pwidth == 3)
		pwidth = 4;
	mem = ((d->width * pwidth + 7) & ~7) * d->height;

	d->x_framebuffer[0] = XCreateImage(d->x_disp, d->x_vis, d->x_visinfo->depth, ZPixmap, 0, Q_Malloc(mem), d->width, d->height, 32, 0);

	if (!d->x_framebuffer[0])
		Sys_Error("VID: XCreateImage failed\n");
}

static void ResetSharedFrameBuffers(struct display *d)
{
	int size, key, minsize = getpagesize(), frm;

	for (frm = 0; frm < 2; frm++)
	{
		// free up old frame buffer memory
		if (d->x_framebuffer[frm])
		{
			XShmDetach(d->x_disp, &d->x_shminfo[frm]);
			free(d->x_framebuffer[frm]);
			shmdt(d->x_shminfo[frm].shmaddr);
		}

		// create the image
		d->x_framebuffer[frm] = XShmCreateImage(d->x_disp, d->x_vis, d->x_visinfo->depth, ZPixmap, 0, &d->x_shminfo[frm], d->width, d->height);

		// grab shared memory
		size = d->x_framebuffer[frm]->bytes_per_line * d->x_framebuffer[frm]->height;
		if (size < minsize)
			Sys_Error("VID: Window must use at least %d bytes\n", minsize);

		key = random();
		d->x_shminfo[frm].shmid = shmget((key_t) key, size, IPC_CREAT | 0777);
		if (d->x_shminfo[frm].shmid == -1)
			Sys_Error("VID: Could not get any shared memory\n");

		// attach to the shared memory segment
		d->x_shminfo[frm].shmaddr = (void *) shmat(d->x_shminfo[frm].shmid, 0, 0);

#if 0
		if (verbose)
			printf("VID: shared memory id=%d, addr=0x%lx\n", d->x_shminfo[frm].shmid, (long) d->x_shminfo[frm].shmaddr);
#endif

		d->x_framebuffer[frm]->data = d->x_shminfo[frm].shmaddr;

		// get the X server to attach to it

		if (!XShmAttach(d->x_disp, &d->x_shminfo[frm]))
			Sys_Error("VID: XShmAttach() failed\n");
		XSync(d->x_disp, 0);
		shmctl(d->x_shminfo[frm].shmid, IPC_RMID, 0);
	}

}

void Sys_Video_CvarInit()
{
	X11_Input_CvarInit();
}

void *Sys_Video_Open(unsigned int width, unsigned int height, unsigned int depth, int fullscreen, unsigned char *palette)
{
	int pnum, i, num_visuals, template_mask;
	XVisualInfo template;
	struct display *d;

	d = malloc(sizeof(*d));
	if (d)
	{
		bzero(d, sizeof(*d));

		d->width = width;
		d->height = height;
		d->depth = depth;
		d->fullscreen = fullscreen;

		srandom(getpid());

		// open the display
		d->x_disp = XOpenDisplay(0);
		if (!d->x_disp)
		{
			if (getenv("DISPLAY"))
				Sys_Error("VID: Could not open display [%s]\n", getenv("DISPLAY"));
			else
				Sys_Error("VID: Could not open local display\n");
		}

		// for debugging only
		XSynchronize(d->x_disp, True);

		d->scrnum = DefaultScreen(d->x_disp);

#if USE_VMODE
		if (fullscreen)
		{
			int version, revision;
			int best_fit, best_dist, dist, x, y;
			int num_vidmodes;

			if (XF86VidModeQueryVersion(d->x_disp, &version, &revision))
			{
				XF86VidModeGetModeLine(d->x_disp, d->scrnum, &d->origvidmode.dotclock, (XF86VidModeModeLine *)&d->origvidmode.hdisplay);

				XF86VidModeGetAllModeLines(d->x_disp, d->scrnum, &num_vidmodes, &d->vidmodes);

				best_dist = 9999999;
				best_fit = -1;

				for (i = 0; i < num_vidmodes; i++)
				{
					if (width > d->vidmodes[i]->hdisplay || height > d->vidmodes[i]->vdisplay)
						continue;

					x = width - d->vidmodes[i]->hdisplay;
					y = height - d->vidmodes[i]->vdisplay;
					dist = x * x + y * y;
					if (dist < best_dist)
					{
						best_dist = dist;
						best_fit = i;
					}
				}

				if (best_fit != -1)
				{
/*
				actualWidth = d->vidmodes[best_fit]->hdisplay;
				actualHeight = d->vidmodes[best_fit]->vdisplay;
*/
					// change to the mode
					XF86VidModeSwitchToMode(d->x_disp, d->scrnum, d->vidmodes[best_fit]);
					d->vidmode_active = true;
					// Move the viewport to top left
					XF86VidModeSetViewPort(d->x_disp, d->scrnum, 0, 0);
				}
				else
				{
					fullscreen = 0;
				}
			}
			else
			{
				Com_Printf("Unable to use the XF86 vidmode extension.\n");
				fullscreen = 0;
			}
		}
#else
		fullscreen = 0;
#endif

		template.visualid = XVisualIDFromVisual(XDefaultVisual(d->x_disp, d->scrnum));
		template_mask = VisualIDMask;

		// pick a visual- warn if more than one was available
		d->x_visinfo = XGetVisualInfo(d->x_disp, template_mask, &template, &num_visuals);
		if (num_visuals > 1)
		{
			printf("Found more than one visual id at depth %d:\n", template.depth);
			for (i = 0; i < num_visuals; i++)
				printf("	-visualid %d\n", (int) (d->x_visinfo[i].visualid));
		}
		else if (num_visuals == 0)
		{
			if (template_mask == VisualIDMask)
				Sys_Error("VID: Bad visual id %d\n", template.visualid);
			else
				Sys_Error("VID: No visuals at depth %d\n", template.depth);
		}

#if 0
		if (verbose)
		{
			printf("Using visualid %d:\n", (int) (d->x_visinfo->visualid));
			printf("	screen %d\n", d->x_visinfo->screen);
			printf("	red_mask 0x%x\n", (int) (d->x_visinfo->red_mask));
			printf("	green_mask 0x%x\n", (int) (d->x_visinfo->green_mask));
			printf("	blue_mask 0x%x\n", (int) (d->x_visinfo->blue_mask));
			printf("	colormap_size %d\n", d->x_visinfo->colormap_size);
			printf("	bits_per_rgb %d\n", d->x_visinfo->bits_per_rgb);
		}
#endif

		d->x_vis = d->x_visinfo->visual;

		// setup attributes for main window
		{
			int attribmask = CWEventMask | CWColormap | CWBorderPixel;
			XSetWindowAttributes attribs;
			Colormap tmpcmap;

			tmpcmap = XCreateColormap(d->x_disp, XRootWindow(d->x_disp, d->x_visinfo->screen), d->x_vis, AllocNone);

			attribs.event_mask = StructureNotifyMask | ExposureMask;
			attribs.border_pixel = 0;
			attribs.colormap = tmpcmap;

			if (fullscreen)
			{
				attribmask = CWColormap | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
				attribs.override_redirect = 1;
				attribs.backing_store = NotUseful;
				attribs.save_under = 0;
			}

// create the main window
			d->x_win = XCreateWindow(d->x_disp, XRootWindow(d->x_disp, d->x_visinfo->screen), 0, 0,	// x, y
					      d->width, d->height, 0,	// borderwidth
					      d->x_visinfo->depth, InputOutput, d->x_vis, attribmask, &attribs);
			XStoreName(d->x_disp, d->x_win, "FodQuake");

			if (d->x_visinfo->class != TrueColor)
				XFreeColormap(d->x_disp, tmpcmap);

		}


		if (d->x_visinfo->depth == 8)
		{
			// create and upload the palette
			if (d->x_visinfo->class == PseudoColor)
			{
				d->x_cmap = XCreateColormap(d->x_disp, d->x_win, d->x_vis, AllocAll);
				Sys_Video_SetPalette(d, palette);
				XSetWindowColormap(d->x_disp, d->x_win, d->x_cmap);
			}

		}

		// inviso cursor
		XDefineCursor(d->x_disp, d->x_win, CreateNullCursor(d->x_disp, d->x_win));

		// create the GC
		{
			XGCValues xgcvalues;
			int valuemask = GCGraphicsExposures;
			xgcvalues.graphics_exposures = False;
			d->x_gc = XCreateGC(d->x_disp, d->x_win, valuemask, &xgcvalues);
		}

		// map the window
		XMapWindow(d->x_disp, d->x_win);

		if (fullscreen)
		{
			XRaiseWindow(d->x_disp, d->x_win);
			XWarpPointer(d->x_disp, None, d->x_win, 0, 0, 0, 0, d->width / 2, d->height / 2);
			XFlush(d->x_disp);
#if USE_VMODE
			XF86VidModeSetViewPort(d->x_disp, d->scrnum, 0, 0);
#endif
		}

		// wait for first exposure event
		{
			XEvent event;
			while(1)
			{
				XNextEvent(d->x_disp, &event);
				if (event.type == Expose && !event.xexpose.count)
					break;
			}
		}
		// now safe to draw

		// even if MITSHM is available, make sure it's a local connection
		if (XShmQueryExtension(d->x_disp))
		{
			char displayname[MAX_OSPATH], *dn;
			d->doShm = true;
			if ((dn = (char *) getenv("DISPLAY")))
			{
				Q_strncpyz(displayname, dn, sizeof(displayname));
				for (dn = displayname; *dn && (*dn != ':'); dn++)
					;
				*dn = 0;
				if (!(!Q_strcasecmp(displayname, "unix") || !*displayname))
					d->doShm = false;
			}
		}
		if (d->doShm)
		{
			d->x_shmeventtype = XShmGetEventBase(d->x_disp) + ShmCompletion;
			ResetSharedFrameBuffers(d);
		}
		else
		{
			Com_Printf("Unable to initialise X11 shared memory support.\n");
			d->x_shmeventtype = 0;
			ResetFrameBuffer(d);
		}

		d->current_framebuffer = 0;

		//XSynchronize(d->x_disp, False);

		shiftmask_init(d);

		d->inputdata = X11_Input_Init(d->x_win, width, height, fullscreen);
		
		return d;
	}

	return 0;
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 2;
}

int Sys_Video_GetKeyEvent(void *display, keynum_t *key, qboolean *down)
{
	struct display *d = display;

	return X11_Input_GetKeyEvent(d->inputdata, key, down);
}

void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

	X11_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	X11_Input_GrabMouse(d->inputdata, d->fullscreen?1:dograb);
}

void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
	struct display *d = display;
	int i;
	XColor colors[256];
	
	for (i = 0; i < 256; i++)
	{
		d->st2d_8to16table[i] = xlib_rgb16(d, palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
		d->st2d_8to24table[i] = xlib_rgb24(d, palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
	}

	if (d->x_visinfo->class == PseudoColor && d->x_visinfo->depth == 8)
	{
		if (palette != d->current_palette)
			memcpy(d->current_palette, palette, 768);
		for (i = 0; i < 256; i++)
		{
			colors[i].pixel = i;
			colors[i].flags = DoRed | DoGreen | DoBlue;
			colors[i].red = palette[i * 3] * 257;
			colors[i].green = palette[i * 3 + 1] * 257;
			colors[i].blue = palette[i * 3 + 2] * 257;
		}
		XStoreColors(d->x_disp, d->x_cmap, colors, 256);
	}
}

// Called at shutdown
void Sys_Video_Close(void *display)
{
	struct display *d = display;

	X11_Input_Shutdown(d->inputdata);
	
#if USE_VMODE
	if (d->vidmode_active)
		XF86VidModeSwitchToMode(d->x_disp, d->scrnum, &d->origvidmode);

	XFree(d->vidmodes);
#endif

	XFree(d->x_visinfo);

	XFreeGC(d->x_disp, d->x_gc);

	XCloseDisplay(d->x_disp);

	free(d);
}

// flushes the given rectangles from the view buffer to the screen

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;
	XEvent event;

	int config_notify;
	int config_notify_width;
	int config_notify_height;

	X11_Input_GetConfigNotify(d->inputdata, &config_notify, &config_notify_width, &config_notify_height);
	
	// if the window changes dimension, skip this frame
	if (config_notify && ((config_notify_width&~7) != d->width || config_notify_height != d->height))
	{
#warning This is broken.
		fprintf(stderr, "config notify\n");
		config_notify = 0;
		d->width = config_notify_width & ~7;
		d->height = config_notify_height;

		if (d->doShm)
			ResetSharedFrameBuffers(d);
		else
			ResetFrameBuffer(d);

		Con_CheckResize();
		return;
	}

	if (d->doShm)
	{
		while (rects)
		{
			if (d->x_visinfo->depth == 24)
				st3_fixup(d, d->x_framebuffer[d->current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			else if (d->x_visinfo->depth == 16)
				st2_fixup(d, d->x_framebuffer[d->current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			if (!XShmPutImage(d->x_disp, d->x_win, d->x_gc, d->x_framebuffer[d->current_framebuffer], rects->x, rects->y, rects->x, rects->y, rects->width, rects->height, True))
				Sys_Error("VID_Update: XShmPutImage failed\n");

			do
			{
				XNextEvent(d->x_disp, &event);
			} while(event.type != d->x_shmeventtype);

			rects = rects->pnext;
		}
		d->current_framebuffer = !d->current_framebuffer;
		XSync(d->x_disp, False);
	}
	else
	{
		while (rects)
		{
			if (d->x_visinfo->depth == 24)
				st3_fixup(d, d->x_framebuffer[d->current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			else if (d->x_visinfo->depth == 16)
				st2_fixup(d, d->x_framebuffer[d->current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			XPutImage(d->x_disp, d->x_win, d->x_gc, d->x_framebuffer[0], rects->x, rects->y, rects->x, rects->y, rects->width, rects->height);
			rects = rects->pnext;
		}
		XSync(d->x_disp, False);
	}
}

void D_BeginDirectRect(int x, int y, byte * pbitmap, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void D_EndDirectRect(int x, int y, int width, int height)
{
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void VID_LockBuffer(void)
{
}

void VID_UnlockBuffer(void)
{
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d;

	d = display;

	XStoreName(d->x_disp, d->x_win, text);
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

	return d->x_framebuffer[0]->bytes_per_line;
}

void *Sys_Video_GetBuffer(void *display)
{
	struct display *d;

	d = display;

	return d->x_framebuffer[d->current_framebuffer]->data;
}

#include "clipboard_x11.c"

