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
// sys.h -- non-portable functions

#ifdef _WIN32
#define Sys_MSleep(x) Sleep(x)
#else
#define Sys_MSleep(x) usleep((x) * 1000)
#endif

// file IO
int	Sys_FileTime (char *path);
void Sys_mkdir (char *path);

// memory protection
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

// an error will cause the entire program to exit
void Sys_Error (char *error, ...);

// send text to the console
void Sys_Printf (char *fmt, ...);

void Sys_Quit (void);

double Sys_DoubleTime (void);
unsigned long long Sys_IntTime(void);

void Sys_SleepTime(unsigned int usec);

char *Sys_ConsoleInput (void);

void Sys_LowFPPrecision (void);
void Sys_HighFPPrecision (void);
void Sys_SetFPCW (void);

void Sys_CvarInit(void);
void Sys_Init (void);

void Sys_Video_CvarInit(void);
void *Sys_Video_Open(unsigned int width, unsigned int height, unsigned int depth, int fullscreen, unsigned char *palette);
void Sys_Video_Close(void *display);
unsigned int Sys_Video_GetNumBuffers(void *display);
void Sys_Video_Update(void *display, vrect_t *rects);
void Sys_Video_SetPalette(void *display, unsigned char *palette);
int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down);
void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey);
void Sys_Video_GrabMouse(void *display, int dograb);
void Sys_Video_SetWindowTitle(void *display, const char *text);
#ifdef GLQUAKE
void Sys_Video_BeginFrame(void *display, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height);
void Sys_Video_SetGamma(void *display, unsigned short *ramps);
qboolean Sys_Video_HWGammaSupported(void *display);
#else
unsigned int Sys_Video_GetWidth(void *display);
unsigned int Sys_Video_GetHeight(void *display);
unsigned int Sys_Video_GetBytesPerRow(void *display);
void *Sys_Video_GetBuffer(void *display);
#endif

const char *Sys_Video_GetClipboardText(void *display);
void Sys_Video_FreeClipboardText(void *display, const char *text);
void Sys_Video_SetClipboardText(void *display, const char *text);

