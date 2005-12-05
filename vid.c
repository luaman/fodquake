#include "quakedef.h"
#include "common.h"
#include "r_shared.h"

static void *display;

void VID_Init(unsigned char *palette)
{
	int width, height, depth;
	int argnum;

#ifdef GLQUAKE
	width = 640;
	height = 480;
	depth = 24;
#else
	width = 320;
	height = 240;
	depth = 8;
#endif
	
	argnum = COM_CheckParm("-width");
	if (argnum)
	{
		if (argnum >= com_argc - 1)
			Sys_Error("VID: -width <width>");

		width = Q_atoi(com_argv[argnum+1]);

		if (width == 0)
			Sys_Error("VID: Bad width");
#warning Fix this.
#ifndef GLQUAKE
		if (width > MAXWIDTH)
			Sys_Error("VID: Maximum supported width is %d", MAXWIDTH);
#endif
	}

	argnum = COM_CheckParm("-height");
	if (argnum)
	{
		if (argnum >= com_argc - 1)
			Sys_Error("VID: -height <height>");

		height = Q_atoi(com_argv[argnum+1]);

		if (height == 0)
			Sys_Error("VID: Bad height");
#warning Fix this.
#ifndef GLQUAKE
		if (height > MAXHEIGHT)
			Sys_Error("VID: Maximum supported height is %d", MAXHEIGHT);
#endif
	}

	argnum = COM_CheckParm("-depth");
	if (argnum)
	{
		if (argnum >= com_argc - 1)
			Sys_Error("VID: -depth <depth>");

		depth = Q_atoi(com_argv[argnum+1]);

		if (depth != 15 && depth != 16 && depth != 24)
			Sys_Error("VID: Bad depth");
	}

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

VID_GetMouseMovement(int *mousex, int *mousey)
{
	Sys_Video_GetMouseMovement(display, mousex, mousey);
}

