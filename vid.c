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

#include "quakedef.h"
#include "common.h"
#ifdef GLQUAKE
#include "gl_local.h"
#include "gl_draw.h"
#include "gl_texture.h"
#else
#include "r_shared.h"
#endif

#ifndef CLIENTONLY
#include "server.h"
#endif

#include "sbar.h"

static void *display;

static char *windowtitle;

qboolean in_grab_windowed_mouse_callback(cvar_t *var, char *value)
{
	printf("in_grab_windowed_mouse changed to %s\n", value);

	if (display)
		Sys_Video_GrabMouse(display, atoi(value));

	return false;
}

#warning Fixme
#ifdef GLQUAKE
cvar_t vid_ref = { "vid_ref", "gl", CVAR_ROM };
#else
cvar_t vid_ref = { "vid_ref", "soft", CVAR_ROM };
#endif

cvar_t vid_fullscreen = { "vid_fullscreen", "1", CVAR_ARCHIVE };
cvar_t vid_width = { "vid_width", "640", CVAR_ARCHIVE };
cvar_t vid_height = { "vid_height", "480", CVAR_ARCHIVE };

#ifdef GLQUAKE
cvar_t vid_depth = { "vid_depth", "24", CVAR_ARCHIVE };
#endif

cvar_t in_grab_windowed_mouse = { "in_grab_windowed_mouse", "1", CVAR_ARCHIVE, in_grab_windowed_mouse_callback };

static unsigned char pal[768];

void VID_Init(unsigned char *palette)
{
	memcpy(pal, palette, sizeof(pal));
}

void VID_Shutdown()
{
	VID_Close();

	free(windowtitle);
}

void VID_Restart_f(void)
{
	int i;

	VID_Close();
#ifdef CLIENTONLY
	Host_ClearMemory();
#else
#ifndef GLQUAKE
	D_FlushCaches();
#endif
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
#ifdef GLQUAKE
	Cvar_Register(&vid_depth);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&in_grab_windowed_mouse);
	Cmd_AddLegacyCommand("_windowed_mouse", "in_grab_windowed_mouse");
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("vid_restart", VID_Restart_f);

	Sys_Video_CvarInit();
}

void VID_Open()
{
	int width, height, depth, fullscreen;
	int i;

	fullscreen = vid_fullscreen.value;
	width = vid_width.value;
	height = vid_height.value;
#ifdef GLQUAKE
	depth = vid_depth.value;
#else
	depth = 8;
#endif

#warning Fix this.
#ifndef GLQUAKE
	if (width > MAXWIDTH)
	{
		Com_Printf("VID: Maximum supported width is %d\n", MAXWIDTH);
		width = MAXWIDTH;
	}
	if (height > MAXHEIGHT)
	{
		Com_Printf("VID: Maximum supported heigfht is %d\n", MAXHEIGHT);
		height = MAXHEIGHT;
	}
#endif

#ifdef GLQUAKE
	if (depth != 15 && depth != 16 && depth != 24)
	{
		Sys_Printf("VID: Bad depth\n");
		depth = 24;
	}
#endif

	vid.colormap = host_colormap;
	vid.width = width;
	vid.height = height;
	vid.aspect = ((float)height / (float)width) * (320.0 / 240.0);

#ifndef GLQUAKE
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
#endif

	display = Sys_Video_Open(width, height, depth, fullscreen, host_basepal);
	if (display == 0)
		Sys_Error("VID: Unable to open a display\n");

	vid.numpages = Sys_Video_GetNumBuffers(display);

#ifdef GLQUAKE
#warning fixme
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

		vid.conwidth &= 0xfff8; // make it a multiple of eight

		if (vid.conwidth < 320)
			vid.conwidth = 320;

		// pick a conheight that matches with correct aspect
		vid.conheight = vid.conwidth * 3 / 4;
	}

	if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
	{
		vid.conheight = Q_atoi(com_argv[i + 1]);

		if (vid.conheight < 200)
			vid.conheight = 200;
	}

	if (vid.conwidth > vid.width)
		vid.conwidth = vid.width;
	
	if (vid.conheight > vid.conheight)
		vid.conheight = vid.height;

	vid.width = vid.conwidth;
	vid.height = vid.conheight;
#else
	vid.rowbytes = Sys_Video_GetBytesPerRow(display);
	vid.buffer = Sys_Video_GetBuffer(display);
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
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

	GL_InitTextureStuff();
	GL_Particles_TextureInit();
	Draw_Init();
	Sbar_Init();
#endif
}

void VID_Close()
{
#ifdef GLQUAKE
	Sbar_Shutdown();
#endif

	if (display)
	{
#ifdef GLQUAKE
		GL_FlushTextures();
		GL_FlushPics();
#endif
		Sys_Video_Close(display);

		display = 0;
	}
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


void VID_GetEvents()
{
	Sys_Video_GetEvents(display);
}

void VID_GetMouseMovement(int *mousex, int *mousey)
{
	Sys_Video_GetMouseMovement(display, mousex, mousey);
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

qboolean VID_IsFullscreen()
{
#warning fixme
	return false;
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

