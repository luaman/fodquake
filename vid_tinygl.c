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

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/extensions.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "gl_local.h"
#include "in_morphos.h"

#ifndef SA_GammaControl
#define SA_GammaControl (SA_Dummy + 123)
#endif

#ifndef SA_3DSupport
#define SA_3DSupport (SA_Dummy + 127)
#endif

#ifndef SA_GammaRed
#define SA_GammaRed (SA_Dummy + 124)
#endif

#ifndef SA_GammaBlue
#define SA_GammaBlue (SA_Dummy + 125)
#endif

#ifndef SA_GammaGreen
#define SA_GammaGreen (SA_Dummy + 126)
#endif

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

struct Library *TinyGLBase = 0;
GLContext *__tglContext;

qboolean vid_hwgamma_enabled = false;

extern viddef_t vid;

cvar_t _windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};

int real_width, real_height;

struct display
{
	void *inputdata;

	struct Screen *screen;
	struct Window *window;

	void *pointermem;

	char pal[256*4];

	unsigned char *gammatable;

};

void Sys_Video_CvarInit(void)
{
}

void *Sys_Video_Open(int width, int height, int depth, int fullscreen, unsigned char *palette)
{
	struct display *d;
	int argnum;

	int r;

	int i;

	Cvar_Register(&_windowed_mouse);

	d = AllocVec(sizeof(*d), MEMF_CLEAR);
	{
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 74))
		{
			d->gammatable = AllocVec(256*3, MEMF_ANY);
			if (d->gammatable)
				vid_hwgamma_enabled = 1;
		}

		vid.width = width;
		vid.height = height;
		vid.maxwarpwidth = WARP_WIDTH;
		vid.maxwarpheight = WARP_HEIGHT;
		vid.numpages = 1;
		vid.colormap = host_colormap;
		vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

		if (vid.width <= 640)
		{
			vid.conwidth = vid.width;
			vid.conheight = vid.height;
		}
		else
		{
			vid.conwidth = vid.width/2;
			vid.conheight = vid.height/2;
		}

		if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
		{
			vid.conwidth = Q_atoi(com_argv[i + 1]);

			// pick a conheight that matches with correct aspect
			vid.conheight = vid.conwidth * 3 / 4;
		}

		vid.conwidth &= 0xfff8; // make it a multiple of eight

		if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
			vid.conheight = Q_atoi(com_argv[i + 1]);

		if (vid.conwidth < 320)
			vid.conwidth = 320;

		if (vid.conheight < 200)
			vid.conheight = 200;

		TinyGLBase = OpenLibrary("tinygl.library", 0);
		if (TinyGLBase)
		{

			vid.rowbytes = vid.width;
			vid.direct = 0; /* Isn't used anywhere, but whatever. */
			vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

			if (fullscreen)
			{
				d->screen = OpenScreenTags(0,
					SA_Width, vid.width,
					SA_Height, vid.height,
					SA_Depth, depth,
					SA_Quiet, TRUE,
					SA_GammaControl, TRUE,
					SA_3DSupport, TRUE,
					TAG_DONE);
			}

			d->window = OpenWindowTags(0,
				WA_InnerWidth, vid.width,
				WA_InnerHeight, vid.height,
				WA_Title, "FodQuake",
				WA_DragBar, d->screen?FALSE:TRUE,
				WA_DepthGadget, d->screen?FALSE:TRUE,
				WA_Borderless, d->screen?TRUE:FALSE,
				WA_RMBTrap, TRUE,
				d->screen?WA_PubScreen:TAG_IGNORE, (ULONG)d->screen,
				WA_Activate, TRUE,
				TAG_DONE);

			if (d->window)
			{
				if (d->screen == 0)
					vid_hwgamma_enabled = 0;

				__tglContext = GLInit();
				if (__tglContext)
				{
					if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
					{
						r = glAInitializeContextScreen(d->screen);
					}
					else
					{
						r = glAInitializeContextWindowed(d->window);
					}

					if (r)
					{
						d->pointermem = AllocVec(256, MEMF_ANY|MEMF_CLEAR);
						if (d->pointermem)
						{
							SetPointer(d->window, d->pointermem, 16, 16, 0, 0);

							real_width = vid.width;
							real_height = vid.height;

							if (vid.conheight > vid.height)
								vid.conheight = vid.height;
							if (vid.conwidth > vid.width)
								vid.conwidth = vid.width;

							vid.width = vid.conwidth;
							vid.height = vid.conheight;

							GL_Init();

							VID_SetPalette(palette);

							vid.recalc_refdef = 1;

							d->inputdata = Sys_Input_Init(d->screen, d->window);
							if (d->inputdata)
							{
								return d;
							}

							FreeVec(d->pointermem);
						}

						if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
							glADestroyContextScreen();
						else
							glADestroyContextWindowed();
					}

					GLClose(__tglContext);
				}

				if (d->screen)
					CloseScreen(d->screen);

				CloseWindow(d->window);
			}

			CloseLibrary(TinyGLBase);
		}

		FreeVec(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	Sys_Input_Shutdown(d->inputdata);

	if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
		glADestroyContextScreen();
	else
		glADestroyContextWindowed();

	GLClose(__tglContext);

	CloseWindow(d->window);

	FreeVec(d->pointermem);

	if (d->screen)
		CloseScreen(d->screen);

	CloseLibrary(TinyGLBase);

	if (d->gammatable)
		FreeVec(d->gammatable);
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

void VID_LockBuffer()
{
}

void VID_UnlockBuffer()
{
}

qboolean VID_IsLocked()
{
	return 0;
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}

void VID_SetCaption(char *text)
{
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = real_width;
	*height = real_height;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;

	glASwapBuffers();
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

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
	struct display *d = display;
	int i;

	if (d->screen && vid_hwgamma_enabled)
	{
		for(i=0;i<768;i++)
		{
			d->gammatable[i] = ramps[i]>>8;
		}

		SetAttrs(d->screen,
			SA_GammaRed, d->gammatable,
			SA_GammaGreen, d->gammatable+256,
			SA_GammaBlue, d->gammatable+512,
			TAG_DONE);
	}
}

/* gl extensions */

void myglMultiTexCoord2fARB(GLenum unit, GLfloat s, GLfloat t)
{
	GLMultiTexCoord2fARB(__tglContext, unit, s, t);
}

void myglActiveTextureARB(GLenum unit)
{
	GLActiveTextureARB(__tglContext, unit);
}

void *tglGetProcAddress(char *s)
{
	if (strcmp(s, "glMultiTexCoord2fARB") == 0)
		return (void *)myglMultiTexCoord2fARB;
	else if (strcmp(s, "glActiveTextureARB") == 0)
		return (void *)myglActiveTextureARB;

	return 0;
}
