/*
Copyright (C) 1996-1997 Id Software, Inc.

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
#include "cdaudio.h"
#include "gl_local.h"
#include "keys.h"
#include "resource.h"
#include "sbar.h"
#include "sound.h"
#include "winquake.h"

struct display {
	HWND window;
	HDC dc;
	int width;
	int height;
	int left;
	int right;
	HGLRC glctx;
	qboolean isfullscreen;
	qboolean isfocused;

	struct display *next;	//this is absurd.
};

static struct display *activedisplays;

#if OLDCODE


#define MAX_MODE_LIST	128
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

static qboolean DDActive;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	*pcurrentmode;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int		windowed_mouse;
extern qboolean	mouseactive;  // from in_win.c
static HICON	hIcon;

static int			DIBWidth, DIBHeight;
static RECT		WindowRect;
static DWORD		WindowStyle, ExWindowStyle;

static HWND		dibwindow;
extern HWND		mainwindow;

static int				vid_modenum = NO_MODE;
static int				vid_default = MODE_WINDOWED;
static int		windowed_default;
static unsigned char	vid_curpal[256*3];

static HGLRC	baseRC;
static HDC		maindc;

static glvert_t glv;

static HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

static modestate_t	modestate = MS_UNINIT;

static unsigned short *currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

static qboolean vid_3dfxgamma = false;
static qboolean vid_gammaworks = false;
static qboolean vid_hwgamma_enabled = false;
static qboolean customgamma = false;

static LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void AppActivate(BOOL fActive, BOOL minimize);
static char *VID_GetModeDescription (int mode);
static void ClearAllStates (void);
static void VID_UpdateWindowStatus (void);

/*********************************** CVARS ***********************************/

static cvar_t		vid_ref = {"vid_ref", "gl", CVAR_ROM};
static cvar_t		vid_mode = {"vid_mode","0"};	// Note that 0 is MODE_WINDOWED
static cvar_t		_vid_default_mode = {"_vid_default_mode","0",CVAR_ARCHIVE};	// Note that 3 is MODE_FULLSCREEN_DEFAULT
static cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3",CVAR_ARCHIVE};
static cvar_t		vid_config_x = {"vid_config_x","800",CVAR_ARCHIVE};
static cvar_t		vid_config_y = {"vid_config_y","600",CVAR_ARCHIVE};
static cvar_t		_windowed_mouse = {"_windowed_mouse","1",CVAR_ARCHIVE};
static cvar_t		vid_displayfrequency = {"vid_displayfrequency", "0", CVAR_INIT};
static cvar_t		vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};


typedef BOOL (APIENTRY *SWAPINTERVALFUNCPTR)(int);
SWAPINTERVALFUNCPTR wglSwapIntervalEXT = NULL;
static qboolean OnChange_vid_vsync(cvar_t *var, char *string);
static qboolean update_vsync = false;
static cvar_t	vid_vsync = {"vid_vsync", "", 0, OnChange_vid_vsync};

static BOOL (APIENTRY *wglGetDeviceGammaRamp3DFX)(HDC hDC, GLvoid *ramp);
static BOOL (APIENTRY *wglSetDeviceGammaRamp3DFX)(HDC hDC, GLvoid *ramp);


int		window_center_x, window_center_y;
static int window_x, window_y, window_width, window_height;
extern RECT	window_rect;

/******************************* GL EXTENSIONS *******************************/


static void GL_WGL_CheckExtensions(void) {
    if (!COM_CheckParm("-noswapctrl") && CheckExtension("WGL_EXT_swap_control")) {
		if ((wglSwapIntervalEXT = (void *) wglGetProcAddress("wglSwapIntervalEXT"))) {
            Com_Printf("Vsync control extensions found\n");
			Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
			Cvar_Register (&vid_vsync);
			Cvar_ResetCurrentGroup();
		}
    }

	if (!COM_CheckParm("-no3dfxgamma") && CheckExtension("WGL_3DFX_gamma_control")) {
		wglGetDeviceGammaRamp3DFX = (void *) wglGetProcAddress("wglGetDeviceGammaRamp3DFX");
		wglSetDeviceGammaRamp3DFX = (void *) wglGetProcAddress("wglSetDeviceGammaRamp3DFX");
		vid_3dfxgamma = (wglGetDeviceGammaRamp3DFX && wglSetDeviceGammaRamp3DFX);
	}
}

static qboolean OnChange_vid_vsync(cvar_t *var, char *string) {
	update_vsync = true;
	return false;
}


static void GL_Init_Win(void) {
	GL_WGL_CheckExtensions();
}

/******************************** WINDOW STUFF ********************************/

static void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify) {
	int CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

static void VID_UpdateWindowStatus (void) {
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

/******************************** VID_SETMODE ********************************/

static qboolean VID_SetWindowedMode (int modenum) {
	HDC hdc;
	int lastmodestate, width, height;
	RECT rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "FodQuake",
		 "FodQuake",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

	// Because we have set the background brush for the window to NULL (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be  empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc, 0, 0, WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}

static qboolean VID_SetFullDIBMode (int modenum) {
	HDC hdc;
	int lastmodestate, width, height;
	RECT rect;

	if (!leavecurrentmode) {
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (vid_displayfrequency.value > 0) {
			gdevmode.dmDisplayFrequency = vid_displayfrequency.value;
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
			Com_DPrintf ("Forcing display frequency to %i Hz\n", gdevmode.dmDisplayFrequency);
		}

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode");
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "FodQuake",
		 "FodQuake",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL (to avoid flickering when re-sizing the window on the desktop),
	// we clear the window to black when created, otherwise it will be  empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = window_y = 0;

	mainwindow = dibwindow;

	SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);

	return true;
}

static int VID_SetMode (int modenum, unsigned char *palette) {
	int original_mode, temp;
	qboolean stat;
    MSG msg;

	if ((windowed && modenum) || (!windowed && modenum < 1) || (!windowed && modenum >= nummodes))
		Sys_Error ("Bad video mode");

	// so Com_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause();

	original_mode = (vid_modenum == NO_MODE) ? windowed_default : vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED) {
		if (_windowed_mouse.value && key_dest == key_game) {
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		} else {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	} else if (modelist[modenum].type == MS_FULLDIB) {
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	} else {
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
		Sys_Error ("Couldn't set video mode");

	// now we try to make sure we get the focus on the mode switch, because sometimes in some systems we don't.
	// We grab the foreground, then finish setting up, pump all our messages, and sleep for a little while
	// to let messages finish bouncing around the system, then we put ourselves at the top of the z order,
	// then grab the foreground again. Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	//	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue (&vid_mode, (float) vid_modenum);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

	//fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Com_Printf ("Video mode %s initialized\n", VID_GetModeDescription (vid_modenum));

	//VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}

#endif
static int bChosePixelFormat(HDC hDC, PIXELFORMATDESCRIPTOR *pfd, PIXELFORMATDESCRIPTOR *retpfd) {
	int pixelformat;

	if (!(pixelformat = ChoosePixelFormat(hDC, pfd))) {
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	if (!(DescribePixelFormat(hDC, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), retpfd))) {
		MessageBox(NULL, "DescribePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	return pixelformat;
}

static BOOL bSetupPixelFormat(HDC hDC) {
	int pixelformat;
	PIXELFORMATDESCRIPTOR retpfd, pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW 				// support window
		|  PFD_SUPPORT_OPENGL 			// support OpenGL
		|  PFD_DOUBLEBUFFER ,			// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		32,								// 32-bit z-buffer
		0,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
	};

	if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
		return FALSE;

	if (retpfd.cDepthBits < 24) {


		pfd.cDepthBits = 24;
		if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
			return FALSE;
	}

	if (!SetPixelFormat(hDC, pixelformat, &retpfd)) {
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (retpfd.cDepthBits < 24)
		gl_allow_ztrick = false;

	return TRUE;
}

#if OLDCODE

/********************************** HW GAMMA **********************************/

static void VID_ShiftPalette (unsigned char *palette) {}

//Note: ramps must point to a static array
static void VID_SetDeviceGammaRamp(unsigned short *ramps) {
	if (!vid_gammaworks)
		return;

	currentgammaramp = ramps;

	if (!vid_hwgamma_enabled)
		return;

	customgamma = true;

	if (Win2K) {
		int i, j;

		for (i = 0; i < 128; i++) {
			for (j = 0; j < 3; j++)
				ramps[j * 256 + i] = min(ramps[j * 256 + i], (i + 0x80) << 8);
		}
		for (j = 0; j < 3; j++)
			ramps[j * 256 + 128] = min(ramps[j * 256 + 128], 0xFE00);
	}

	if (vid_3dfxgamma)
		wglSetDeviceGammaRamp3DFX(maindc, ramps);
	else
		SetDeviceGammaRamp(maindc, ramps);
}

static void InitHWGamma (void) {
	if (COM_CheckParm("-nohwgamma"))
		return;
	if (vid_3dfxgamma)
		vid_gammaworks = wglGetDeviceGammaRamp3DFX(maindc, systemgammaramp);
	else
		vid_gammaworks = GetDeviceGammaRamp(maindc, systemgammaramp);
}

static void RestoreHWGamma(void) {
	if (vid_gammaworks && customgamma) {
		customgamma = false;
		if (vid_3dfxgamma)
			wglSetDeviceGammaRamp3DFX(maindc, systemgammaramp);
		else
			SetDeviceGammaRamp(maindc, systemgammaramp);
	}
}

/*********************************** OPENGL ***********************************/

static void GL_BeginRendering (unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height) {
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
}

static void GL_EndRendering (void) {
	static qboolean old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks && ActiveApp && !Minimized;
	vid_hwgamma_enabled = vid_hwgamma_enabled && (modestate == MS_FULLDIB || vid_hwgammacontrol.value == 2);
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	if (!scr_skipupdate || block_drawing) {

		if (wglSwapIntervalEXT && update_vsync && vid_vsync.string[0])
			wglSwapIntervalEXT(vid_vsync.value ? 1 : 0);
		update_vsync = false;

		SwapBuffers(maindc);
	}

	// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
}

/******************************** VID SHUTDOWN ********************************/

static void VID_Shutdown (void) {
   	HGLRC hRC;
   	HDC hDC;

	if (vid_initialized) {
		RestoreHWGamma ();

		vid_canalttab = false;
		hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

    	wglMakeCurrent(NULL, NULL);

		if (hRC)
			wglDeleteContext(hRC);

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);
	}
}

/************************ SIGNALS, MESSAGES, INPUT ETC ************************/

static void IN_ClearStates (void);

static void ClearAllStates (void) {
	extern qboolean keydown[256];
	int i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++) {
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

static void AppActivate(BOOL fActive, BOOL minimize) {
/******************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
******************************************************************************/
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

	// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active) {
//		S_BlockSound ();
		sound_active = false;
	} else if (ActiveApp && !sound_active) {
//		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive) {
		if (vid_canalttab && !Minimized && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);

		if (modestate == MS_FULLDIB) {
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
					Sys_Error ("Couldn't set fullscreen DIB mode");
				ShowWindow (mainwindow, SW_SHOWNORMAL);

				// Fix for alt-tab bug in NVidia drivers
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);

				// scr_fullupdate = 0;
				Sbar_Changed ();
			}
		} else if (modestate == MS_WINDOWED && Minimized) {
			ShowWindow (mainwindow, SW_RESTORE);
		} else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game) {
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
	} else {
		RestoreHWGamma ();
		if (modestate == MS_FULLDIB) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		} else if ((modestate == MS_WINDOWED) && _windowed_mouse.value) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}

static LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static int IN_MapKey (int key);


#include "mw_hook.h"
static void MW_Hook_Message (long buttons);


/* main window procedure */
static LONG WINAPI MainWndProc (HWND    hWnd, UINT    uMsg, WPARAM  wParam, LPARAM  lParam) {
    LONG lRet = 1;
	int fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if (uMsg == uiWheelMessage) {
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

    switch (uMsg) {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (IN_MapKey(lParam), true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (IN_MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;
		// this is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;


		case WM_MWHOOK:
			MW_Hook_Message (lParam);
			break;


    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "FodQuake : Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Host_Quit ();
			}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

			// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
			if (dibwindow)
				DestroyWindow (dibwindow);

            PostQuitMessage (0);
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}

/********************************* VID MODES *********************************/

static int VID_NumModes (void) {
	return nummodes;
}

static vmode_t *VID_GetModePtr (int modenum) {
	if (modenum >= 0 && modenum < nummodes)
		return &modelist[modenum];

	return &badmode;
}

static char *VID_GetModeDescription (int mode) {
	char *pinfo;
	vmode_t *pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode) {
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	} else {
		sprintf (temp, "Desktop resolution (%dx%d)", modelist[MODE_FULLSCREEN_DEFAULT].width, modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

static char *VID_GetExtModeDescription (int mode) {
	static char	pinfo[40];
	vmode_t *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB) {
		if (!leavecurrentmode) {
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		} else {
			sprintf (pinfo, "Desktop resolution (%dx%d)",
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	} else {
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			strcpy(pinfo, "windowed");
	}

	return pinfo;
}

static void VID_ModeList_f (void) {
	int i, lnummodes, t;
	char *pinfo;
	vmode_t *pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i = 1; i < lnummodes; i++) {
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Com_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}

/********************************** VID INIT **********************************/

static void VID_InitDIB (HINSTANCE hInstance) {
	int temp;
	WNDCLASS wc;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "FodQuake";

    if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if ((temp = COM_CheckParm("-width")) && temp + 1 < com_argc)
		modelist[0].width = Q_atoi(com_argv[temp + 1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if ((temp = COM_CheckParm("-height")) && temp + 1 < com_argc)
		modelist[0].height= Q_atoi(com_argv[temp + 1]);
	else
		modelist[0].height = modelist[0].width * 240 / 320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	sprintf (modelist[0].modedesc, "%dx%d", modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}

static void VID_InitFullDIB (HINSTANCE hInstance) {
	DEVMODE	devmode;
	int i, modenum, originalnummodes, numlowresmodes, j, bpp, done;
	BOOL stat;

	// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do {
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if (devmode.dmBitsPerPel >= 15 && devmode.dmPelsWidth <= MAXWIDTH && devmode.dmPelsHeight <= MAXHEIGHT && nummodes < MAX_MODE_LIST) {
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				// if the width is more than twice the height, reduce it by half because this is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect")) {
					if (modelist[nummodes].width > (modelist[nummodes].height << 1)) {
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							modelist[nummodes].width, modelist[nummodes].height, modelist[nummodes].bpp);
					}
				}

				for (i = originalnummodes; i < nummodes; i++) {
					if (modelist[nummodes].width == modelist[i].width &&
						modelist[nummodes].height == modelist[i].height &&
						modelist[nummodes].bpp == modelist[i].bpp)
					{
						break;
					}
				}
				if (i == nummodes)
					nummodes++;
			}
		}
		modenum++;
	} while (stat);

	// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do {
		for (j = 0; j < numlowresmodes && nummodes < MAX_MODE_LIST; j++) {
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				for (i = originalnummodes; i < nummodes; i++) {
					if (modelist[nummodes].width == modelist[i].width &&
						modelist[nummodes].height == modelist[i].height &&
						modelist[nummodes].bpp == modelist[i].bpp)
					{
						break;
					}
				}

				if (i == nummodes)
					nummodes++;
			}
		}
		switch (bpp) {
			case 16:
				bpp = 32; break;
			case 32:
				bpp = 24; break;
			case 24:
				done = 1; break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Com_Printf ("No fullscreen DIB modes found\n");
}

static void VID_Init (unsigned char *palette) {
	int i, temp, basenummodes, width, height, bpp, findbpp, done;
	HDC hdc;
	DEVMODE	devmode;

	memset(&devmode, 0, sizeof(devmode));

	Cmd_AddCommand ("vid_modelist", VID_ModeList_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed")) {
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			Sys_Error ("Can't run in non-RGB mode");

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	} else {
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if ((temp = COM_CheckParm("-mode")) && temp + 1 < com_argc) {
			vid_default = Q_atoi(com_argv[temp + 1]);
		} else if (COM_CheckParm("-current")) {
			modelist[MODE_FULLSCREEN_DEFAULT].width = GetSystemMetrics (SM_CXSCREEN);
			modelist[MODE_FULLSCREEN_DEFAULT].height = GetSystemMetrics (SM_CYSCREEN);
			vid_default = MODE_FULLSCREEN_DEFAULT;
			leavecurrentmode = 1;
		} else {
			if ((temp = COM_CheckParm("-width")) && temp + 1 < com_argc)
				width = Q_atoi(com_argv[temp + 1]);
			else
				width = 640;

			if ((temp = COM_CheckParm("-bpp")) && temp + 1 < com_argc) {
				bpp = Q_atoi(com_argv[temp + 1]);
				findbpp = 0;
			} else {
				bpp = 15;
				findbpp = 1;
			}

			if ((temp = COM_CheckParm("-height")) && temp + 1 < com_argc)
				height = Q_atoi(com_argv[temp + 1]);

			// if they want to force it, add the specified mode to the list
			if (COM_CheckParm("-force") && nummodes < MAX_MODE_LIST) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = width;
				modelist[nummodes].height = height;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = bpp;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel);

				for (i = nummodes; i < nummodes; i++) {
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						break;
					}
				}

				if (i == nummodes)
					nummodes++;
			}

			done = 0;

			do {
				height = 0;
				if ((temp = COM_CheckParm("-height")) && temp + 1 < com_argc)
					height = Q_atoi(com_argv[temp + 1]);
				else
					height = 0;

				for (i = 1, vid_default = 0; i < nummodes; i++) {
					if (modelist[i].width == width && (!height || modelist[i].height == height) && modelist[i].bpp == bpp) {
						vid_default = i;
						done = 1;
						break;
					}
				}

				if (findbpp && !done) {
					switch (bpp) {
					case 15:
						bpp = 16; break;
					case 16:
						bpp = 32; break;
					case 32:
						bpp = 24; break;
					case 24:
						done = 1; break;
					}
				} else {
					done = 1;
				}
			} while (!done);

			if (!vid_default)
				Sys_Error ("Specified video mode not available");
		}
	}

	vid_initialized = true;

	if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
		vid.conwidth = Q_atoi(com_argv[i + 1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
		vid.conheight = Q_atoi(com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	vid.colormap = host_colormap;
	Check_Gamma(palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	maindc = GetDC(mainwindow);
	if (!bSetupPixelFormat(maindc))
		Sys_Error ("bSetupPixelFormat failed");

	InitHWGamma ();

	if (!(baseRC = wglCreateContext(maindc)))
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent(maindc, baseRC))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init();
	GL_Init_Win();

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;
}


#endif















LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int IN_MapKey (int key);


#include "mw_hook.h"
void MW_Hook_Message (long buttons);


static void ClearAllStates (void)
{
	extern qboolean keydown[256];
	int i;

	// send an up event for each key, to make sure the server clears them all
	for (i = 0; i < 256; i++)
	{
		if (keydown[i])
			Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

/* main window procedure */
static LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG lRet = 1;
	int fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	struct display *d;

	for (d = activedisplays; d; d = d->next)
	{
		if (d->window == hWnd)
			break;
	}
	if (!d)
	{
		//this shouldn't ever happen
		return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

	if (uMsg == uiWheelMessage)
	{
		uMsg = WM_MOUSEWHEEL;
		wParam <<= 16;
	}

	switch (uMsg)
	{
		case WM_KILLFOCUS:
			if (d->isfullscreen)
				ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			d->left = (int) LOWORD(lParam);
			d->right = (int) HIWORD(lParam);
//			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (IN_MapKey(lParam), true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (IN_MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;
		// this is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1)
				temp |= 8;

			if (wParam & MK_XBUTTON2)
				temp |= 16;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD(wParam) > 0)
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			}
			else
			{
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;


		case WM_MWHOOK:
			MW_Hook_Message (lParam);
			break;


		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "FodQuake : Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Host_Quit ();
			}
			break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			d->isfocused = !(fActive == WA_INACTIVE);

			// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

		case WM_DESTROY:
			DestroyWindow (hWnd);
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}






static cvar_t		vid_ref = {"vid_ref", "gl", CVAR_ROM};
/*
static cvar_t		vid_mode = {"vid_mode","0"};	// Note that 0 is MODE_WINDOWED
static cvar_t		_vid_default_mode = {"_vid_default_mode","0",CVAR_ARCHIVE};	// Note that 3 is MODE_FULLSCREEN_DEFAULT
static cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3",CVAR_ARCHIVE};
static cvar_t		vid_config_x = {"vid_config_x","800",CVAR_ARCHIVE};
static cvar_t		vid_config_y = {"vid_config_y","600",CVAR_ARCHIVE};
static cvar_t		_windowed_mouse = {"_windowed_mouse","1",CVAR_ARCHIVE};
*/
static cvar_t		vid_displayfrequency = {"vid_displayfrequency", "0", CVAR_INIT};
/*
static cvar_t		vid_vsync = {"vid_vsync", "", CVAR_INIT};
static cvar_t		vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};
*/
void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d = display;
	SetWindowText (d->window, text);
}
	


/*void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

}*/

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	if (dograb)
	{
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		IN_DeactivateMouse ();
		IN_ShowMouse ();
	}
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
//noop, for now
}

void Sys_Video_BeginFrame(void *display, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height)
{
	struct display *d = display;

	*x = 0;
	*y = 0;
	*width = d->width;
	*height = d->height;
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 3;
}

void Sys_Video_Update(void *display, struct vrect *rect)
{
	struct display *d = display;
	SwapBuffers(d->dc);
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	if (d->isfullscreen)
		ChangeDisplaySettings (NULL, 0);
#warning shiesh, nothing here
}

void Sys_Video_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register (&vid_ref);
/*	Cvar_Register (&vid_mode);
	Cvar_Register (&_vid_default_mode);
	Cvar_Register (&_vid_default_mode_win);
	Cvar_Register (&vid_config_x);
	Cvar_Register (&vid_config_y);
	Cvar_Register (&_windowed_mouse);
	Cvar_Register (&vid_hwgammacontrol);
*/	Cvar_Register (&vid_displayfrequency);
/*
	Cvar_Register (&vid_vsync);
*/
	Cvar_ResetCurrentGroup();
}

void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	RECT		rect;
	int WindowStyle, ExWindowStyle;
	struct display *d;
	HANDLE hdc;
	WNDCLASS wc;
	int displacement;
	int depth;

	depth = 24;

#warning this function doesn't clean up yet

	if (fullscreen)	//first step is to set up the video res that we want
	{
		DEVMODE gdevmode;
		memset(&gdevmode, 0, sizeof(gdevmode));
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = ((depth==24)?32:depth);	//windows does not like 24bit depth (although that is all we get with 32bit anyway)
		gdevmode.dmPelsWidth = width;
		gdevmode.dmPelsHeight = height;
		gdevmode.dmSize = sizeof (gdevmode);

#warning display frequency should be a parameter to this function (so that we can tell if its already failed once or not)
		if (vid_displayfrequency.value > 0)
		{
			gdevmode.dmDisplayFrequency = vid_displayfrequency.value;
			gdevmode.dmFields |= DM_DISPLAYFREQUENCY;
			Com_DPrintf ("Forcing display frequency to %i Hz\n", gdevmode.dmDisplayFrequency);
		}

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Com_Printf ("Couldn't set fullscreen DIB mode\n");

#if GOWINDOWEDINSTEAD
			fullscreen = false;
#else
			return NULL;
#endif
		}
	}

	if (!fullscreen)
	{
		hdc = GetDC (NULL);
		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Com_Printf ("You probably don't want opengl paletted. Switch to 16bit or 32bit colour or something.\n");
			return NULL;
		}
		ReleaseDC (NULL, hdc);
	}

	d = malloc(sizeof(*d));
	if (!d)
	{
		Com_Printf("malloc failed\n");
		return NULL;
	}

	memset(d, 0, sizeof(*d));
	d->width = width;
	d->height = height;

	d->next = activedisplays;
	activedisplays = d;

	d->isfullscreen = fullscreen;


	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = global_hInstance;
	wc.hIcon         = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "FodQuake";

	RegisterClass (&wc);	//assume that any failures are due to it still being registered
					//we'll fail on the create instead.


	rect.top = rect.left = 0;
	rect.right = d->width;
	rect.bottom = d->height;


	if (d->isfullscreen)
	{
		WindowStyle = WS_POPUP;
		ExWindowStyle = 0;
	}
	else
	{
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		ExWindowStyle = 0;
	}

	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	//ajust the rect so that it appears in the center of the screen
	displacement = (GetSystemMetrics(SM_CYSCREEN) - (rect.top+rect.bottom)) / 2;
	rect.top += displacement;
	rect.bottom += displacement;
	displacement = (GetSystemMetrics(SM_CXSCREEN) - (rect.left+rect.right)) / 2;
	rect.left += displacement;
	rect.right += displacement;

	// Create the DIB window
	d->window = CreateWindowEx (
		 ExWindowStyle,
		 "FodQuake",
		 "FodQuake",
		 WindowStyle,
		 rect.left, rect.top,
		 rect.right - rect.left,
		 rect.bottom - rect.top,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!d->window)
	{
		Com_Printf("Couldn't create a window\n");
		return NULL;
	}


	ShowWindow (d->window, SW_SHOWDEFAULT);
	UpdateWindow (d->window);


	d->dc = GetDC(d->window);
	if (!bSetupPixelFormat(d->dc))
	{
		Com_Printf ("bSetupPixelFormat failed\n");
		return NULL;
	}

	if (!(d->glctx = wglCreateContext(d->dc)))
	{
		Com_Printf ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.\n");
		return NULL;
	}
	if (!wglMakeCurrent(d->dc, d->glctx))
	{
		Com_Printf ("wglMakeCurrent failed\n");
		return NULL;
	}

	GL_Init();
	VID_SetPalette(palette);

mainwindow = d->window;

	vid.width = vid.conwidth = d->width;
	vid.height = vid.conheight = d->height;

	window_rect = rect;
	window_center_x = (window_rect.left + window_rect.right)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;

	IN_StartupMouse();

	return d;
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	return false;	//for now
}

const char *Sys_Video_GetClipboardText(void *display)
{
	return NULL;
}
void Sys_Video_FreeClipboardText(void *display, const char *text)	//is text technically a const here?...
{
}
void Sys_Video_SetClipboardText(void *display, const char *text)
{
}

unsigned int Sys_Video_GetWidth(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

qboolean Sys_Video_GetFullscreen(void *display)
{
	struct display *d;

	d = display;

	return d->isfullscreen;
}

//REMOVE THESE
int window_center_x, window_center_y;
RECT	window_rect;

HWND mainwindow;
char *Sys_GetClipboardData(void)
{
	return NULL;
}
