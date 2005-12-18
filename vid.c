#include "quakedef.h"
#include "common.h"
#include "r_shared.h"

static void *display;

#warning Fixme
cvar_t vid_ref = { "vid_ref", "soft", CVAR_ROM };

cvar_t vid_width = { "vid_width", "640", CVAR_ARCHIVE };
cvar_t vid_height = { "vid_height", "480", CVAR_ARCHIVE };

#ifdef GLQUAKE
cvar_t vid_depth = { "vid_depth", "24", CVAR_ARCHIVE };
#endif

void VID_Init(unsigned char *palette)
{
}

void VID_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register(&vid_ref);
	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);
#ifdef GLQUAKE
	Cvar_Register(&vid_depth);
#endif
	Cvar_ResetCurrentGroup();
}

void VID_Open()
{
	int width, height, depth;

#ifdef GLQUAKE
	width = 640;
	height = 480;
	depth = 24;
#else
	width = 320;
	height = 240;
	depth = 8;
#endif

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

	display = Sys_Video_Open(width, height, depth, host_basepal);
	if (display == 0)
		Sys_Error("VID: Unable to open a display\n");
}

void VID_Shutdown()
{
	Sys_Video_Close(display);
}

void VID_Update (vrect_t *rects)
{
	Sys_Video_Update(display, rects);
}

void VID_SetPalette(byte *palette)
{
	Sys_Video_SetPalette(display, palette);
}

void VID_GetEvents()
{
	Sys_Video_GetEvents(display);
}

void VID_GetMouseMovement(int *mousex, int *mousey)
{
	Sys_Video_GetMouseMovement(display, mousex, mousey);
}

