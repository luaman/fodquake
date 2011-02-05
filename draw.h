/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2005-2007, 2009-2011 Mark Olsen

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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifndef DRAW_H
#define DRAW_H

#include "wad.h"

#ifdef GLQUAKE
typedef struct
{
	int			width;
	short			height;
	int			texnum;
	float		sl, tl, sh, th;
} mpic_t;
#else
typedef struct
{
	int			width;
	short		height;
	byte		alpha;
	byte		pad;
	byte		data[4];	// variable sized
} mpic_t;
#endif

void Draw_CvarInit(void);
void Draw_Init (void);
#ifdef GLQUAKE
void Draw_InitGL(void);
void Draw_ShutdownGL(void);
#endif
void Draw_BeginTextRendering(void);
void Draw_EndTextRendering(void);
void Draw_Character (int x, int y, int num);
void Draw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_Pic (int x, int y, mpic_t *pic);
void Draw_TransPic (int x, int y, mpic_t *pic);
void Draw_TransPicTranslate (int x, int y, mpic_t *pic, byte *translation);
void Draw_ConsoleBackground (int lines);
void Draw_TileClear (int x, int y, int w, int h);
void Draw_Fill (int x, int y, int w, int h, int c);
void Draw_FadeScreen (void);
void Draw_String (int x, int y, const char *str);
void Draw_String_Length(int x, int y, const char *str, int len);
void Draw_Alt_String (int x, int y, const char *str);
void Draw_ColoredString (int x, int y, char *str, int red);
void Draw_ColoredString_Length(int x, int y, char *text, int red, int len, unsigned short startcolour);
mpic_t *Draw_CachePic (char *path);
mpic_t *Draw_CacheWadPic (char *name);
void Draw_FreeWadPic(mpic_t *pic);
void Draw_Crosshair(void);
void Draw_TextBox (int x, int y, int width, int lines);

#endif

