#include <exec/exec.h>
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

GLContext *__tglContext;

static int glctx = 0;

extern viddef_t vid;

struct Screen *screen = 0;
struct Window *window = 0;

static void *pointermem;

struct Library *TinyGLBase = 0;

qboolean vid_hwgamma_enabled = false;

char pal[256*4];

unsigned char *gammatable;

cvar_t _windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};

static unsigned int lastwindowedmouse;

int real_width, real_height;

void VID_Init(int width, int height, int depth, unsigned char *palette)
{
	int argnum;

	int r;

	int i;

	Cvar_Register(&_windowed_mouse);

	if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 74))
	{
		gammatable = AllocVec(256*3, MEMF_ANY);
		if (gammatable)
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
	if (TinyGLBase == 0)
	{
		Sys_Error("VID: Couldn't open tinygl.library");
	}

	vid.rowbytes = vid.width;
	vid.direct = 0; /* Isn't used anywhere, but whatever. */
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	argnum = COM_CheckParm("-window");

	if (argnum == 0)
	{
		screen = OpenScreenTags(0,
			SA_Width, vid.width,
			SA_Height, vid.height,
			SA_Depth, depth,
			SA_Quiet, TRUE,
			SA_GammaControl, TRUE,
			SA_3DSupport, TRUE,
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
		TAG_DONE);

	if (window == 0)
		Sys_Error("Unable to open window");

	if (screen == 0)
		vid_hwgamma_enabled = 0;

	__tglContext = GLInit();
	if (__tglContext == 0)
	{
		Sys_Error("Unable to create GL context");
	}

	if (screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
	{
		r = glAInitializeContextScreen(screen);
	}
	else
	{
		r = glAInitializeContextWindowed(window);
	}

	if (r == 0)
	{
		Sys_Error("Unable to initialize GL context");
	}

	glctx = 1;

	pointermem = AllocVec(256, MEMF_ANY|MEMF_CLEAR);
	if (pointermem == 0)
	{
		Sys_Error("Unable to allocate memory for mouse pointer");
	}

	SetPointer(window, pointermem, 16, 16, 0, 0);

	lastwindowedmouse = 1;

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
}

void VID_Shutdown()
{
	if (glctx)
	{
		if (screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
		{
			glADestroyContextScreen();
		}
		else
		{
			glADestroyContextWindowed();
		}

		glctx = 0;
	}

	if (__tglContext)
	{
		GLClose(__tglContext);
		__tglContext = 0;
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

	if (TinyGLBase)
	{
		CloseLibrary(TinyGLBase);
		TinyGLBase = 0;
	}

	if (gammatable)
	{
		FreeVec(gammatable);
		gammatable = 0;
	}
}

void VID_ShiftPalette(unsigned char *p)
{
	VID_SetPalette(p);
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

void GL_EndRendering(void)
{
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

	glASwapBuffers();
}

void VID_SetDeviceGammaRamp(unsigned short *ramps)
{
	int i;

	if (vid_hwgamma_enabled)
	{
		for(i=0;i<768;i++)
		{
			gammatable[i] = ramps[i]>>8;
		}

		SetAttrs(screen,
			SA_GammaRed, gammatable,
			SA_GammaGreen, gammatable+256,
			SA_GammaBlue, gammatable+512,
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

	printf("tglGetProcAddress(\"%s\")\n", s);

	return 0;
}
