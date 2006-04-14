#include "quakedef.h"
#include "common.h"
#ifndef GLQUAKE
#include "r_shared.h"
#endif

static void *display;

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

static unsigned char pal[768];

void VID_Init(unsigned char *palette)
{
	memcpy(pal, palette, sizeof(pal));
}

void VID_Restart_f(void)
{
	VID_Shutdown();
	VID_Open();
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
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("vid_restart", VID_Restart_f);
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

#ifdef GLQUAKE
	VID_SetPalette(pal);

	GL_InitTextureStuff();
	GL_Particles_TextureInit();
	Draw_Init();
	Sbar_Init();
#else
	Sys_Video_SetPalette(display, pal);
#endif
}

void VID_Shutdown()
{
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

