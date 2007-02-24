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
#ifndef GLQUAKE
#include "r_shared.h"
#endif

#ifndef CLIENTONLY
#include "server.h"
#endif

#include "sbar.h"

static void *display;

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

void VID_Restart_f(void)
{
	int i;

	VID_Shutdown();
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

	fullscreen = vid_fullscreen.value;
	width = vid_width.value;
	height = vid_height.value;
#ifdef GLQUAKE
	depth = vid_depth.value;
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

	display = Sys_Video_Open(width, height, depth, fullscreen, host_basepal);
	if (display == 0)
		Sys_Error("VID: Unable to open a display\n");

	Sys_Video_GrabMouse(display, in_grab_windowed_mouse.value);

	V_UpdatePalette(true);
#ifdef GLQUAKE
	GL_InitTextureStuff();
	GL_Particles_TextureInit();
	Draw_Init();
	Sbar_Init();
#endif
}

void VID_Shutdown()
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

void VID_Update (vrect_t *rects)
{
	Sys_Video_Update(display, rects);
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
#endif

