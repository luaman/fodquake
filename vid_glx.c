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

#ifndef __CYGWIN__
#define USE_VMODE 1
#endif

#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include "quakedef.h"
#include "keys.h"
#include "input.h"
#include "gl_local.h"

#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#if USE_VMODE
#include <X11/extensions/xf86vmode.h>
#endif

#include "in_x11.h"

struct display
{
	void *inputdata;

	unsigned int width, height;

#ifdef USE_VMODE
	XF86VidModeModeInfo **vidmodes;
	XF86VidModeModeInfo origvidmode;
	qboolean vidmode_active;
	qboolean customgamma;
#endif
	Display *x_disp;
	Window x_win;
	GLXContext ctx;
	int scrnum;
	int hasfocus;

	int fullscreen;

	qboolean vid_gammaworks;
	unsigned short systemgammaramp[3][256];
	unsigned short *currentgammaramp;
};


#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask)

static void RestoreHWGamma(struct display *d);

static void eventcallback(void *display, int type)
{
	struct display *d = display;
	switch(type)
	{
		case FocusIn:
			d->hasfocus = 1;
			V_UpdatePalette(true);
			break;
		case FocusOut:
			d->hasfocus = 0;
			RestoreHWGamma(display);
			break;
	}
}

void Sys_Video_GetEvents(void *display)
{
	struct display *d = display;

	X11_Input_GetEvents(d->inputdata);
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

/************************************* COMPATABILITY *************************************/

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d;

	d = display;

	XStoreName(d->x_disp, d->x_win, text);
}

void signal_handler(int sig) {
	printf("Received signal %d, exiting...\n", sig);
	VID_Shutdown();
	Sys_Quit();
	exit(0);
}

void InitSig(void) {
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/************************************* HW GAMMA *************************************/

static void InitHWGamma(struct display *d)
{
#if USE_VMODE
	int xf86vm_gammaramp_size;

	if (!d->fullscreen)
		return;

	XF86VidModeGetGammaRampSize(d->x_disp, d->scrnum, &xf86vm_gammaramp_size);
	
	d->vid_gammaworks = (xf86vm_gammaramp_size == 256);

	if (d->vid_gammaworks)
	{
		XF86VidModeGetGammaRamp(d->x_disp, d->scrnum, xf86vm_gammaramp_size, d->systemgammaramp[0], d->systemgammaramp[1], d->systemgammaramp[2]);
	}
#endif
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
#if USE_VMODE
	struct display *d = display;

	if (d->vid_gammaworks && d->hasfocus)
	{
		d->currentgammaramp = ramps;
		XF86VidModeSetGammaRamp(d->x_disp, d->scrnum, 256, ramps, ramps + 256, ramps + 512);
		d->customgamma = true;
	}
#endif
}

static void RestoreHWGamma(struct display *d)
{
#if USE_VMODE
	if (d->vid_gammaworks && d->customgamma)
	{
		d->customgamma = false;
		XF86VidModeSetGammaRamp(d->x_disp, d->scrnum, 256, d->systemgammaramp[0], d->systemgammaramp[1], d->systemgammaramp[2]);
	}
#endif
}

/************************************* GL *************************************/

void Sys_Video_BeginFrame(void *display, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height)
{
	struct display *d;

	d = display;

	*x = 0;
	*y = 0;
	*width = d->width;
	*height = d->height;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;

	glFlush();
	glXSwapBuffers(d->x_disp, d->x_win);

	glXWaitGL();
}

/************************************* VID SHUTDOWN *************************************/

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	X11_Input_Shutdown(d->inputdata);

	RestoreHWGamma(d);

	glXDestroyContext(d->x_disp, d->ctx);
	XDestroyWindow(d->x_disp, d->x_win);

#ifdef USE_VMODE
	if (d->vidmode_active)
		XF86VidModeSwitchToMode(d->x_disp, d->scrnum, &d->origvidmode);
#endif

	XCloseDisplay(d->x_disp);

	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 2;
}

/************************************* VID INIT *************************************/

static Cursor CreateNullCursor(Display *display, Window root) {
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(display, root, 1, 1, 1);
	xgc.function = GXclear;
	gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask,
		&dummycolour,&dummycolour, 0,0);
	XFreePixmap(display,cursormask);
	XFreeGC(display,gc);
	return cursor;
}

void Sys_Video_CvarInit()
{
	X11_Input_CvarInit();
}

void *Sys_Video_Open(unsigned int width, unsigned int height, unsigned int depth, int fullscreen, unsigned char *palette)
{
	struct display *d;

	int attrib[] =
	{
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
	int i;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;

	d = malloc(sizeof(*d));
	if (d)
	{
		bzero(d, sizeof(*d));

		d->hasfocus = 1;
		d->fullscreen = fullscreen;
	d->x_disp = XOpenDisplay(NULL);
	if (d->x_disp)
	{
		d->scrnum = DefaultScreen(d->x_disp);
		visinfo = glXChooseVisual(d->x_disp, d->scrnum, attrib);
		if (visinfo)
		{

			root = RootWindow(d->x_disp, d->scrnum);

#ifdef USE_VMODE
			if (fullscreen)
			{
				int version, revision;
				int best_fit, best_dist, dist, x, y;
				int num_vidmodes;

				if (XF86VidModeQueryVersion(d->x_disp, &version, &revision))
				{
					Com_Printf("Using XF86-VidModeExtension Ver. %d.%d\n", version, revision);

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
						actualWidth = vidmodes[best_fit]->hdisplay;
						actualHeight = vidmodes[best_fit]->vdisplay;
*/
						// change to the mode
						XF86VidModeSwitchToMode(d->x_disp, d->scrnum, d->vidmodes[best_fit]);
						d->vidmode_active = true;
						// Move the viewport to top left
						XF86VidModeSetViewPort(d->x_disp, d->scrnum, 0, 0);
					}
					else
						fullscreen = 0;
				}
				else
					fullscreen = 0;
			}
#else
			fullscreen = 0;
#endif

			// window attributes
			attr.background_pixel = 0;
			attr.border_pixel = 0;
			attr.colormap = XCreateColormap(d->x_disp, root, visinfo->visual, AllocNone);
			attr.event_mask = X_MASK;
			mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

			if (fullscreen)
			{
				mask = CWBackPixel | CWColormap | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
				attr.override_redirect = True;
				attr.backing_store = NotUseful;
				attr.save_under = False;
			}

			d->x_win = XCreateWindow(d->x_disp, root, 0, 0, width, height,0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

			XStoreName(d->x_disp, d->x_win, "FodQuake");

			XDefineCursor(d->x_disp, d->x_win, CreateNullCursor(d->x_disp, d->x_win));

			XMapWindow(d->x_disp, d->x_win);

#if USE_VMODE
			if (fullscreen)
			{
				XRaiseWindow(d->x_disp, d->x_win);
				XWarpPointer(d->x_disp, None, d->x_win, 0, 0, 0, 0, 0, 0);
				XFlush(d->x_disp);
				// Move the viewport to top left
				XF86VidModeSetViewPort(d->x_disp, d->scrnum, 0, 0);
				XGrabKeyboard(d->x_disp, d->x_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);
			}
#endif

			XFlush(d->x_disp);

			d->ctx = glXCreateContext(d->x_disp, visinfo, NULL, True);

			glXMakeCurrent(d->x_disp, d->x_win, d->ctx);

			d->width = width;
			d->height = height;

			InitSig(); // trap evil signals

			InitHWGamma(d);

			Com_Printf ("Video mode %dx%d initialized.\n", width, height);

			if (fullscreen)
				vid_windowedmouse = false;

			d->inputdata = X11_Input_Init(d->x_disp, d->x_win, 0, eventcallback, d);
			if (d->inputdata)
				return d;
		}

		XCloseDisplay(d->x_disp);
	}
	free(d);
	}

	return 0;
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	struct display *d;

	d = display;

	return d->vid_gammaworks;
}

#include "clipboard_x11.c"

