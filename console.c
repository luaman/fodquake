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
// console.c

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "quakedef.h"
#include "keys.h"

#include "ignore.h"
#include "logging.h"
#include "readablechars.h"

#include "context_sensitive_tab.h"

#define		MINIMUM_CONBUFSIZE	(1 << 15)
#define		DEFAULT_CONBUFSIZE	(1 << 16)
#define		MAXIMUM_CONBUFSIZE	(1 << 22)

typedef struct
{
	char *text;
	int maxsize;
	int current;         // line where next message will be printed
	int x;               // offset in current line for next print
	int display;         // bottom of console displays this line
	int numlines;        // number of non-blank text lines, used for backscroling
} console_t;

console_t	con;

static int con_ormask;
static int con_linewidth;		// characters across screen
static int con_totallines;		// total lines in console scrollback
static float con_cursorspeed = 4;

static cvar_t _con_notifylines = {"con_notifylines","4"};
static cvar_t con_notifytime = {"con_notifytime","3"};         //seconds
static cvar_t con_wordwrap = {"con_wordwrap","1"};

#define	NUM_CON_TIMES 16
static float con_times[NUM_CON_TIMES];  // cls.realtime time the line was generated
                                        // for transparent notify lines

static int con_vislines;

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

static qboolean con_initialized = false;
static qboolean con_suppress = false;

static FILE		*qconsole_log;

static void Con_Clear_f(void)
{
	con.numlines = 0;
	memset(con.text, ' ', con.maxsize);
	con.display = con.current;
}

void Con_ClearNotify(void)
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

//If the line width has changed, reformat the buffer
void Con_CheckResize(unsigned int pixelwidth)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char *tempbuf;

	width = (pixelwidth >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1) // video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = con.maxsize / con_linewidth;
		memset (con.text, ' ', con.maxsize);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = con.maxsize / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		tempbuf = malloc(con.maxsize);
		if (tempbuf)
			memcpy (tempbuf, con.text, con.maxsize);

		memset (con.text, ' ', con.maxsize);

		if (tempbuf)
		{
			for (i = 0; i < numlines; i++)
			{
				for (j = 0; j < numchars; j++)
				{
					con.text[(con_totallines - 1 - i) * con_linewidth + j] =
						tempbuf[((con.current - i + oldtotallines) % oldtotallines) * oldwidth + j];
				}
			}

			free(tempbuf);
		}

		Con_ClearNotify ();
	}

	con.current = con_totallines - 1;
	con.display = con.current;
}


static void Con_InitConsoleBuffer(console_t *conbuffer, int size)
{
	con.maxsize = size;
	con.text = malloc(con.maxsize);
	if (con.text == 0)
		Sys_Error("Con_InitConsoleBuffer: Out of memory\n");
}

void Con_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&_con_notifylines);
	Cvar_Register (&con_notifytime);
	Cvar_Register (&con_wordwrap);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("clear", Con_Clear_f);
}

void Con_Init(void)
{
	int i, conbufsize;

	if (dedicated)
		return;

	if (COM_CheckParm("-condebug"))
		qconsole_log = fopen(va("%s/qw/qconsole.log",com_basedir), "a");

	if ((i = COM_CheckParm("-conbufsize")) && i + 1 < com_argc)
	{
		conbufsize = Q_atoi(com_argv[i + 1]) << 10;
		conbufsize = bound (MINIMUM_CONBUFSIZE , conbufsize, MAXIMUM_CONBUFSIZE);
	}
	else
	{
		conbufsize = DEFAULT_CONBUFSIZE;
	}

	Con_InitConsoleBuffer(&con, conbufsize);

	con_linewidth = -1;
	Con_CheckResize(vid.width);

	con_initialized = true;
	Com_Printf ("Console initialized\n");
}

void Con_Shutdown(void)
{
	if (qconsole_log)
		fclose(qconsole_log);

	free(con.text);
}

static void Con_Linefeed(void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;

	con.current++;
	if (con.numlines < con_totallines)
		con.numlines++;

	memset (&con.text[(con.current%con_totallines)*con_linewidth], ' ', con_linewidth);

	// mark time for transparent overlay
	if (con.current >= 0)
		con_times[con.current % NUM_CON_TIMES] = cls.realtime;
}

//Handles cursor positioning, line wrapping, etc
void Con_Print(const char *txt)
{
	int y, c, l, mask;
	static int cr;

	if (qconsole_log)
	{
		fprintf(qconsole_log, "%s", txt);
		fflush(qconsole_log);
	}

	if (Log_IsLogging())
	{
		if (log_readable.value)
		{
			char *s, *tempbuf;

			tempbuf = Z_Malloc(strlen(txt) + 1);
			strcpy(tempbuf, txt);
			for (s = tempbuf; *s; s++)
				*s = readablechars[(unsigned char) *s];
			Log_Write(tempbuf);
			Z_Free(tempbuf);
		}
		else
		{
			Log_Write(txt);	
		}
	}

	if (!con_initialized || con_suppress)
		return;

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
	{
		mask = 0;
	}

	while ((c = *txt))
	{
		// count word length
		for (l = 0; l < con_linewidth; l++)
		{
			char d = txt[l] & 127;
			if ((con_wordwrap.value && (!txt[l] || d == 0x09 || d == 0x0D || d == 0x0A || d == 0x20)) ||
			    (!con_wordwrap.value && txt[l] <= ' '))
			{
				break;
			}
		}

		// word wrap
		if (l != con_linewidth && con.x + l > con_linewidth)
			con.x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		if (!con.x)
			Con_Linefeed ();

		switch (c)
		{
			case '\n':
				con.x = 0;
				break;

			case '\r':
				con.x = 0;
				cr = 1;
				break;

			default:	// display character and advance
				if (con.x >= con_linewidth)
					Con_Linefeed();

				y = con.current % con_totallines;
				con.text[y * con_linewidth+con.x] = c | mask | con_ormask;
				con.x++;
				break;
		}
	}
}

/*
==============================================================================
DRAWING
==============================================================================
*/

//The input line scrolls horizontally if typing goes beyond the right edge
static void Con_DrawInput(void)
{
	int len;
	char *text, temp[MAXCMDLINE + 1];	//+ 1 for cursor if strlen(key_lines[edit_line]) == (MAX_CMDLINE - 1)

	if (key_dest != key_console && cls.state == ca_active)
		return;

	Q_strncpyz(temp, key_lines[edit_line], MAXCMDLINE);
	len = strlen(temp);
	text = temp;

	memset(text + len, ' ', MAXCMDLINE - len);		// fill out remainder with spaces
	text[MAXCMDLINE] = 0;

	// add the cursor frame
	if ((int) (curtime * con_cursorspeed) & 1)
		text[key_linepos] = 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	Draw_String(8, con_vislines - 22, text);
}

//Draws the last few lines of output transparently over the game top
unsigned int Con_DrawNotify (void)
{
	int v, maxlines, i;
	char *text;
	float time;

	Draw_BeginTextRendering();

	maxlines = _con_notifylines.value;
	if (maxlines > NUM_CON_TIMES)
		maxlines = NUM_CON_TIMES;
	if (maxlines < 0)
		maxlines = 0;

	v = 0;
	for (i = con.current-maxlines + 1; i <= con.current; i++)
	{
		if (i < 0)
			continue;

		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;

		time = cls.realtime - time;
		if (time > con_notifytime.value)
			continue;

		text = con.text + (i % con_totallines)*con_linewidth;

		scr_copytop = 1;

		Draw_String_Length(8, v, text, con_linewidth);

		v += 8;
	}

	Draw_EndTextRendering();

	return v / 8;
}

//Draws the console with the solid background
void Con_DrawConsole(int lines)
{
	int i, j, x, y, n, rows, row;
	char *text, dlbar[1024];

	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

	// draw the text
	con_vislines = lines;

	// changed to line things up better
	rows = (lines - 22) >> 3;		// rows of text to draw

	y = lines - 30;

	row = con.display;

	// draw from the bottom up
	if (con.display != con.current)
	{
		// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con_linewidth; x += 4)
			Draw_Character ((x + 1) << 3, y, '^');

		y -= 8;
		rows--;
		row--;
	}

	for (i = 0; i < rows; i++, y -= 8, row--)
	{
		if (row < 0)
			break;

		if (con.current - row >= con_totallines)
			break;		// past scrollback wrap point

		text = con.text + (row % con_totallines)*con_linewidth;

		Draw_String_Length(8, y, text, con_linewidth);
	}

	// draw the download bar
	// figure out width
	if (cls.download)
	{
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con_linewidth - ((con_linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con_linewidth/3;
		if (strlen(text) > i)
		{
			y = x - i - 11;
			Q_strncpyz (dlbar, text, i+1);
			strcat(dlbar, "...");
		}
		else
		{
			strcpy(dlbar, text);
		}
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;

		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		y = con_vislines - 22 + 8;
		Draw_String (8, y, dlbar);
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput();
	Context_Sensitive_Tab_Completion_Draw();
}

void Con_Suppress(void)
{
	con_suppress = true;
}

void Con_Unsuppress(void)
{
	con_suppress = false;
}

unsigned int Con_GetColumns()
{
	return con_linewidth;
}

void Con_ScrollUp(unsigned int numlines)
{
	con.display -= numlines;
	if (con.display - con.current + con.numlines < 0)
		con.display = con.current - con.numlines;
}

void Con_ScrollDown(unsigned int numlines)
{
	con.display += numlines;
	if (con.display - con.current > 0)
		con.display = con.current;
}

void Con_Home()
{
	con.display = con.current - con.numlines;
}

void Con_End()
{
	con.display = con.current;
}

