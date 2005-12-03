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

extern viddef_t vid;

char *buffer = 0;
short *zbuffer = 0;
char *sbuffer = 0;

struct Screen *screen = 0;
struct Window *window = 0;

static struct ScreenBuffer *screenbuffers[3] = { 0, 0, 0 };
static int currentbuffer;

static void *pointermem;

static struct RastPort rastport;

char pal[256*4];

cvar_t _windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};

static unsigned int lastwindowedmouse;

void Sys_Video_Init(int width, int height, int depth, unsigned char *palette)
{
	int argnum;
	int i;

	Cvar_Register(&_windowed_mouse);

	vid.width = width;
	vid.height = height;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	buffer = AllocVec(vid.width*vid.height, MEMF_ANY);
	if (buffer == 0)
	{
		Sys_Error("VID: Couldn't allocate frame buffer");
	}

	zbuffer = AllocVec(vid.width*vid.height*sizeof(*d_pzbuffer), MEMF_ANY);
	if (zbuffer == 0)
	{
		Sys_Error("VID: Couldn't allocate zbuffer");
	}

	sbuffer = AllocVec(D_SurfaceCacheForRes(vid.width, vid.height), MEMF_ANY);
	if (sbuffer == 0)
	{
		Sys_Error("VID: Couldn't allocate surface cache");
	}

	D_InitCaches(sbuffer, D_SurfaceCacheForRes(vid.width, vid.height));

	d_pzbuffer = zbuffer;

	vid.rowbytes = vid.width;
	vid.buffer = buffer;
	vid.direct = 0; /* Isn't used anywhere, but whatever. */
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	argnum = COM_CheckParm("-window");

	if (argnum == 0)
	{
		screen = OpenScreenTags(0,
			SA_Width, vid.width,
			SA_Height, vid.height,
			SA_Depth, 8,
			SA_Quiet, TRUE,
			TAG_DONE);
	}
	

	window = OpenWindowTags(0,
		WA_InnerWidth, vid.width,
		WA_InnerHeight, vid.height,
		WA_Title, "Fuhquake",
		WA_DragBar, screen?FALSE:TRUE,
		WA_DepthGadget, screen?FALSE:TRUE,
		WA_Borderless, screen?TRUE:FALSE,
		WA_RMBTrap, TRUE,
		screen?WA_PubScreen:TAG_IGNORE, (ULONG)screen,
		WA_Activate, TRUE,
		WA_ReportMouse, TRUE,
		TAG_DONE);

	if (window == 0)
		Sys_Error("Unable to open window");

	pointermem = AllocVec(256, MEMF_ANY|MEMF_CLEAR);
	if (pointermem == 0)
	{
		Sys_Error("Unable to allocate memory for mouse pointer");
	}

	SetPointer(window, pointermem, 16, 16, 0, 0);

	lastwindowedmouse = 1;

	vid.numpages = screen?3:1;

	if (screen)
	{
		for(i=0;i<3;i++)
		{
			screenbuffers[i] = AllocScreenBuffer(screen, 0, i?SB_COPY_BITMAP:SB_SCREEN_BITMAP);
			if (screenbuffers[i] == 0)
			{
				Sys_Error("Unable to set up triplebuffering");
			}
		}
	}

	currentbuffer = 0;

	InitRastPort(&rastport);

	VID_SetPalette(palette);
}

void VID_Shutdown()
{
	int i;

	if (buffer)
	{
		FreeVec(buffer);
		buffer = 0;
	}

	if (zbuffer)
	{
		FreeVec(zbuffer);
		zbuffer = 0;
	}

	if (sbuffer)
	{
		FreeVec(sbuffer);
		sbuffer = 0;
	}

	for(i=0;i<3;i++)
	{
		if (screenbuffers[i])
		{
			FreeScreenBuffer(screen, screenbuffers[i]);
			screenbuffers[i] = 0;
		}
	}

	if (window)
	{
		CloseWindow(window);
		window = 0;
	}

	if (pointermem)
	{
		FreeVec(pointermem);
		pointermem = 0;
	}

	if (screen)
	{
		CloseScreen(screen);
		screen = 0;
	}
}

void VID_Update(vrect_t *rects)
{
	while(rects)
	{
		if (screen)
		{
			rastport.BitMap = screenbuffers[currentbuffer]->sb_BitMap;
			WritePixelArray(buffer, rects->x, rects->y, vid.rowbytes, &rastport, rects->x, rects->y, rects->width, rects->height, RECTFMT_LUT8);
		}
		else
		{
			WriteLUTPixelArray(buffer, rects->x, rects->y, vid.rowbytes, window->RPort, pal, window->BorderLeft+rects->x, window->BorderTop+rects->y, rects->width, rects->height, CTABFMT_XRGB8);
		}


		rects = rects->pnext;
	}

	if (screen)
	{
		ChangeScreenBuffer(screen, screenbuffers[currentbuffer]);
		currentbuffer++;
		currentbuffer%= 3;
	}

	/* Check for the windowed mouse setting here */
	if (lastwindowedmouse != _windowed_mouse.value && !screen)
	{
		lastwindowedmouse = _windowed_mouse.value;

		if (lastwindowedmouse == 1)
		{
			/* Hide pointer */

			SetPointer(window, pointermem, 16, 16, 0, 0);
		}
		else
		{
			/* Show pointer */

			ClearPointer(window);
		}
	}
}

void VID_ShiftPalette(unsigned char *p)
{
	VID_SetPalette(p);
}

void VID_SetPalette(unsigned char *palette)
{
	int i;

	ULONG spal[1+(256*3)+1];

	if (screen)
	{
		spal[0] = 256<<16;

		for(i=0;i<256;i++)
		{
			spal[1+(i*3)] = ((unsigned int)palette[i*3])<<24;
			spal[2+(i*3)] = ((unsigned int)palette[i*3+1])<<24;
			spal[3+(i*3)] = ((unsigned int)palette[i*3+2])<<24;
		}

		spal[1+(3*256)] = 0;

		LoadRGB32(&screen->ViewPort, spal);
	}

	for(i=0;i<256;i++)
	{
		pal[i*4] = 0;
		pal[i*4+1] = palette[i*3+0];
		pal[i*4+2] = palette[i*3+1];
		pal[i*4+3] = palette[i*3+2];
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

qboolean VID_IsLocked()
{
	return false;
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

