/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2010 Mark Olsen

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
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "tableprint.h"

#define COLUMNWIDTH 20
#define MINCOLUMNWIDTH 18	// the last column may be slightly smaller

struct TablePrint
{
	int x;
};

struct TablePrint *TablePrint_Begin(int dosort)
{
	struct TablePrint *tp;

	tp = malloc(sizeof(*tp));
	if (tp)
		tp->x = 0;

	return tp;
}

void TablePrint_AddItem(struct TablePrint *tp, const char *txt)
{
	int nextcolx = 0;
	int con_linewidth;

	if (tp == 0)
		return;

	con_linewidth = Con_GetColumns();

	if (tp->x)
		nextcolx = (int)((tp->x + COLUMNWIDTH)/COLUMNWIDTH)*COLUMNWIDTH;

	if (nextcolx > con_linewidth - MINCOLUMNWIDTH || (tp->x && nextcolx + strlen(txt) >= con_linewidth))
	{
		Con_Print("\n");
		tp->x = 0;
	}

	if (tp->x)
	{
		Con_Print(" ");
		tp->x++;
	}

	while (tp->x % COLUMNWIDTH)
	{
		Con_Print(" ");
		tp->x++;
	}

	Con_Print(txt);
	tp->x += strlen(txt);
}

void TablePrint_End(struct TablePrint *tp)
{
	if (tp == 0)
	{
		Con_Print("Out of memory\n");
		return;
	}

	if (tp->x)
		Con_Print("\n");
	
	free(tp);
}


