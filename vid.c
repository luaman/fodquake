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

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "common.h"
#ifdef GLQUAKE
#include "gl_local.h"
#include "gl_draw.h"
#include "gl_texture.h"
#else
#include "r_shared.h"
#include "d_local.h"
#endif

#ifndef CLIENTONLY
#include "server.h"
#endif

#include "sys_thread.h"

#include "sbar.h"

static struct SysMutex *display_mutex;

static void *display;

static char *windowtitle;

#ifdef GLQUAKE
static void set_up_conwidth_conheight(void);

static qboolean vid_conwidth_callback(cvar_t *var, char *value)
{
	var->value = Q_atof(value);

	set_up_conwidth_conheight();

	return false;
}

static qboolean vid_conheight_callback(cvar_t *var, char *value)
{
	var->value = Q_atof(value);

	set_up_conwidth_conheight();

	return false;
}
#endif

static qboolean in_grab_windowed_mouse_callback(cvar_t *var, char *value)
{
	if (display)
		Sys_Video_GrabMouse(display, atoi(value));

	return false;
}

#warning Fixme
#ifdef GLQUAKE
static cvar_t vid_ref = { "vid_ref", "gl", CVAR_ROM };
#else
static cvar_t vid_ref = { "vid_ref", "soft", CVAR_ROM };
#endif

cvar_t vid_fullscreen = { "vid_fullscreen", "1", CVAR_ARCHIVE };
cvar_t vid_width = { "vid_width", "640", CVAR_ARCHIVE };
cvar_t vid_height = { "vid_height", "480", CVAR_ARCHIVE };
cvar_t vid_mode = { "vid_mode", "", CVAR_ARCHIVE };

#ifdef GLQUAKE
cvar_t vid_conwidth = { "vid_conwidth", "0", CVAR_ARCHIVE, vid_conwidth_callback };
cvar_t vid_conheight = { "vid_conheight", "0", CVAR_ARCHIVE, vid_conheight_callback };
#endif

cvar_t in_grab_windowed_mouse = { "in_grab_windowed_mouse", "1", CVAR_ARCHIVE, in_grab_windowed_mouse_callback };

static unsigned char pal[768];

#ifndef GLQUAKE
static void *vid_surfcache;
#endif

static void set_up_conwidth_conheight()
{
	if (display)
	{
		vid.width = Sys_Video_GetWidth(display);
		vid.height = Sys_Video_GetHeight(display);
	}
	else
	{
		vid.width = 320;
		vid.height = 240;
	}

#ifdef GLQUAKE
	if (vid.width <= 640 || vid.height < 400)
	{
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
	}
	else
	{
		vid.conwidth = vid.width/2;
		vid.conheight = vid.height/2;
	}

	if (vid_conwidth.value)
	{
		vid.conwidth = vid_conwidth.value;

		vid.conwidth &= 0xfff8; // make it a multiple of eight

		if (vid.conwidth < 320)
			vid.conwidth = 320;

		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth * 3 / 4;
	}

	if (vid_conheight.value)
	{
		vid.conheight = vid_conheight.value;

		if (vid.conheight < 200)
			vid.conheight = 200;
	}

	if (vid.conwidth > vid.width)
		vid.conwidth = vid.width;
	
	if (vid.conheight > vid.height)
		vid.conheight = vid.height;

	vid.width = vid.conwidth;
	vid.height = vid.conheight;
#else
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
#endif

	vid.recalc_refdef = 1;
}

void VID_Init(unsigned char *palette)
{
	memcpy(pal, palette, sizeof(pal));

	display_mutex = Sys_Thread_CreateMutex();
	if (display_mutex)
	{
		return;
	}
}

void VID_Shutdown()
{
	VID_Close();

	Sys_Thread_DeleteMutex(display_mutex);

	free(windowtitle);
	windowtitle = 0;
}

void VID_Restart(void)
{
	int i;

	if (!display)
		return;

#ifndef GLQUAKE
	D_FlushCaches();
#endif

	VID_Close();
#ifdef CLIENTONLY
	Host_ClearMemory();
#else
	Mod_ClearAll();
	Hunk_FreeToLowMark(server_hunklevel);
#endif
	VID_Open();
	
	for(i=1;i < MAX_MODELS;i++)
	{
		if (cl.model_precache[i])
			Mod_LoadModel(cl.model_precache[i], true);
	}

	if (cl.model_precache[1])
		R_NewMap();

	Skin_Reload();
}

void VID_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register(&vid_ref);
	Cvar_Register(&vid_fullscreen);
	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);
	Cvar_Register(&vid_mode);
#ifdef GLQUAKE
	Cvar_Register(&vid_conwidth);
	Cvar_Register(&vid_conheight);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&in_grab_windowed_mouse);
	Cmd_AddLegacyCommand("_windowed_mouse", "in_grab_windowed_mouse");
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("vid_restart", VID_Restart);

	Sys_Video_CvarInit();
}

#ifndef GLQUAKE
static int VID_SW_AllocBuffers(int width, int height)
{
	unsigned int surfcachesize;

	surfcachesize = D_SurfaceCacheForRes(width, height);

	d_pzbuffer = malloc(width * height * sizeof(*d_pzbuffer));
	if (d_pzbuffer)
	{
		vid_surfcache = malloc(surfcachesize);
		if (vid_surfcache)
		{
			D_InitCaches(vid_surfcache, surfcachesize);

			return 1;
		}

		free(d_pzbuffer);
	}

	return 0;
}

static void VID_SW_FreeBuffers()
{
	D_FlushCaches();

	free(vid_surfcache);
	free(d_pzbuffer);
}
#endif

void VID_Open()
{
	int width, height, fullscreen;

	fullscreen = vid_fullscreen.value;
	width = vid_width.value;
	height = vid_height.value;

#warning Fix this.
#ifndef GLQUAKE
	if (width > MAXWIDTH)
	{
		Com_Printf("VID: Maximum supported width is %d\n", MAXWIDTH);
		width = MAXWIDTH;
	}
	if (height > MAXHEIGHT)
	{
		Com_Printf("VID: Maximum supported height is %d\n", MAXHEIGHT);
		height = MAXHEIGHT;
	}
#endif

	vid.colormap = host_colormap;
	vid.aspect = ((float)height / (float)width) * (320.0 / 240.0);

#ifndef GLQUAKE
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
#endif

	Sys_Thread_LockMutex(display_mutex);
	display = Sys_Video_Open(vid_mode.string, width, height, fullscreen, host_basepal);
	Sys_Thread_UnlockMutex(display_mutex);
	if (display)
	{
		width = Sys_Video_GetWidth(display);
		height = Sys_Video_GetHeight(display);
#ifndef GLQUAKE
		if (VID_SW_AllocBuffers(width, height))
#endif
		{
			vid.numpages = Sys_Video_GetNumBuffers(display);

			set_up_conwidth_conheight();

#ifndef GLQUAKE
			vid.rowbytes = Sys_Video_GetBytesPerRow(display);
			vid.buffer = Sys_Video_GetBuffer(display);
#endif

			if (windowtitle)
				Sys_Video_SetWindowTitle(display, windowtitle);

			Sys_Video_GrabMouse(display, in_grab_windowed_mouse.value);

			V_UpdatePalette(true);
#ifdef GLQUAKE
			GL_Init();

			Check_Gamma(host_basepal);
			VID_SetPalette(host_basepal);

			vid.recalc_refdef = 1;				// force a surface cache flush

			R_InitGL();
			GL_Particles_TextureInit();
			Draw_InitGL();
			Sbar_Init();
			SCR_LoadTextures();
#endif

			return;
		}

		Sys_Thread_LockMutex(display_mutex);

		Sys_Video_Close(display);

		display = 0;

		Sys_Thread_UnlockMutex(display_mutex);
	}

	Sys_Error("VID: Unable to open a display\n");
}

void VID_Close()
{
	Sys_Thread_LockMutex(display_mutex);
#ifdef GLQUAKE
	Sbar_Shutdown();
	Draw_ShutdownGL();
#endif

	if (display)
	{
#ifdef GLQUAKE
		GL_FlushTextures();
		GL_FlushPics();
#endif
		Sys_Video_Close(display);

#ifndef GLQUAKE
		VID_SW_FreeBuffers();
#endif

		display = 0;
	}

	Sys_Thread_UnlockMutex(display_mutex);
}

#ifdef GLQUAKE
#warning Should fix this junk some day.
void VID_BeginFrame(int *x, int *y, int *width, int *height)
{
	Sys_Video_BeginFrame(display, x, y, width, height);
}
#endif

void VID_Update(vrect_t *rects)
{
	Sys_Video_Update(display, rects);

#ifndef GLQUAKE
#warning Fixme, this is a sucky place to put this.
	vid.buffer = Sys_Video_GetBuffer(display);
#endif
}

#ifndef GLQUAKE
void VID_SetPalette(byte *palette)
{
	memcpy(pal, palette, sizeof(pal));
	Sys_Video_SetPalette(display, palette);
}
#endif


int VID_GetKeyEvent(keynum_t *key, qboolean *down)
{
	return Sys_Video_GetKeyEvent(display, key, down);
}

void VID_GetMouseMovement(int *mousex, int *mousey)
{
	Sys_Thread_LockMutex(display_mutex);

	if (display)
		Sys_Video_GetMouseMovement(display, mousex, mousey);
	else
	{
		*mousex = 0;
		*mousey = 0;
	}

	Sys_Thread_UnlockMutex(display_mutex);
}

#ifdef GLQUAKE
void VID_SetDeviceGammaRamp(unsigned short *ramps)
{
	Sys_Video_SetGamma(display, ramps);
}

qboolean VID_HWGammaSupported()
{
	return Sys_Video_HWGammaSupported(display);
}
#endif

void VID_SetCaption(const char *text)
{
	char *newwindowtitle;

	if (display)
	{
		newwindowtitle = malloc(strlen(text)+1);
		if (newwindowtitle)
		{
			strcpy(newwindowtitle, text);
			free(windowtitle);
			windowtitle = newwindowtitle;
		}

		Sys_Video_SetWindowTitle(display, text);
	}
}

const char *VID_GetClipboardText()
{
	return Sys_Video_GetClipboardText(display);
}

void VID_FreeClipboardText(const char *text)
{
	Sys_Video_FreeClipboardText(display, text);
}

void VID_SetClipboardText(const char *text)
{
	Sys_Video_SetClipboardText(display, text);
}

unsigned int VID_GetWidth()
{
	return Sys_Video_GetWidth(display);
}

unsigned int VID_GetHeight()
{
	return Sys_Video_GetHeight(display);
}

qboolean VID_GetFullscreen()
{
	return Sys_Video_GetFullscreen(display);
}

const char *VID_GetMode()
{
	if (Sys_Video_GetFullscreen(display))
		return Sys_Video_GetMode(display);

	return "";
}

