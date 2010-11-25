/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "quakedef.h"
#include "readablechars.h"
#include "utils.h"

int TP_CategorizeMessage (char *s, int *offset);
char *COM_FileExtension (char *in);

/************************************** General Utils **************************************/

char *SecondsToMinutesString(int print_time)
{
	static char time[128];
	int tens_minutes, minutes, tens_seconds, seconds;

	tens_minutes = fmod (print_time / 600, 6);
	minutes = fmod (print_time / 60, 10);
	tens_seconds = fmod (print_time / 10, 6);
	seconds = fmod (print_time, 10);
	Q_snprintfz (time, sizeof(time), "%i%i:%i%i", tens_minutes, minutes, tens_seconds, seconds);
	return time;
}

char *SecondsToHourString(int print_time)
{
	static char time[128];
	int tens_hours, hours,tens_minutes, minutes, tens_seconds, seconds;

	tens_hours = fmod (print_time / 36000, 10);
	hours = fmod (print_time / 3600, 10);
	tens_minutes = fmod (print_time / 600, 6);
	minutes = fmod (print_time / 60, 10);
	tens_seconds = fmod (print_time / 10, 6);
	seconds = fmod (print_time, 10);
	Q_snprintfz (time, sizeof(time), "%i%i:%i%i:%i%i", tens_hours, hours, tens_minutes, minutes, tens_seconds, seconds);
	return time;
}

#ifdef GLQUAKE
byte *StringToRGB(char *s)
{
	byte *col;
	static byte rgb[4];

	Cmd_TokenizeString(s);
	if (Cmd_Argc() == 3)
	{
		rgb[0] = (byte) Q_atoi(Cmd_Argv(0));
		rgb[1] = (byte) Q_atoi(Cmd_Argv(1));
		rgb[2] = (byte) Q_atoi(Cmd_Argv(2));
	}
	else
	{
		col = (byte *) &d_8to24table[(byte) Q_atoi(s)];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}
	rgb[3] = 255;
	return rgb;
}
#endif

/************************************** File Utils **************************************/

int Util_Extend_Filename(char *filename, char **ext)
{
	char extendedname[1024], **s;
	int i, offset;
	FILE *f;

	Q_strncpyz(extendedname, filename, sizeof(extendedname));
	offset = strlen(extendedname);

	i = -1;
	while(1)
	{
		if (++i == 1000)
			break;
		for (s = ext; *s; s++)
		{
			Q_snprintfz (extendedname + offset, sizeof(extendedname) - offset, "_%03i.%s", i, *s);
			if ((f = fopen(extendedname, "rb")))
			{
				fclose(f);
				break;
			}
		}
		if (!*s)
			return i;
	}
	return -1;
}

void Util_Process_Filename(char *string)
{
	int i;

	if (!string)
		return;

	for (i = 0; i < strlen(string); i++)
		if (string[i] == '\\')
			string[i] = '/';
	if (string[0] == '/')
		for (i = 1; i <= strlen(string); i++)
			string[i - 1] = string[i];
}

qboolean Util_Is_Valid_Filename(char *s)
{
	static const char badchars[] = { '?', '*', ':', '<', '>', '"', 0 };

	if (!s || !*s)
		return false;

	if (strstr(s, "../") || strstr(s, "..\\") )
		return false;

	while (*s)
	{
		if (*s < 32 || *s >= 127 || strchr(badchars, *s))
			return false;
		s++;
	}
	return true;
}

char *Util_Invalid_Filename_Msg(char *s)
{
	static char err[192];

	if (!s)
		return NULL;

	Q_snprintfz(err, sizeof(err), "%s is not a valid filename (?*:<>\" are illegal characters)\n", s);
	return err;
}

/************************************* Player Utils *************************************/

static int Player_Compare (const void *p1, const void *p2)
{
	player_info_t *player1, *player2;
	int team_comp;

    player1 = *((player_info_t **) p1);
	player2 = *((player_info_t **) p2);

	if (player1->spectator)
		return player2->spectator ? (player1 - player2) : 1;
	if (player2->spectator)
		return -1;

	if (cl.teamplay && (team_comp = strcmp(player1->team, player2->team)))
		return team_comp;

	return (player1 - player2);
}

int Player_NumtoSlot (int num)
{
	int count, i;
	player_info_t *players[MAX_CLIENTS];

	for (count = i = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0])
			players[count++] = &cl.players[i];

	qsort(players, count, sizeof(player_info_t *), Player_Compare);

	if (num < 1 || num > count)
		return PLAYER_NUM_NOMATCH;

	for (i = 0; i < MAX_CLIENTS; i++)
		if (&cl.players[i] == players[num-1])
			return i;

	return PLAYER_NUM_NOMATCH;
}

int Player_IdtoSlot (int id)
{
	int j;

	for (j = 0; j < MAX_CLIENTS; j++)
	{
		if (cl.players[j].name[0] && cl.players[j].userid == id)
			return j;
	}
	return -1;
}

int Player_StringtoSlot(char *arg)
{
	int i, slot;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && !strncmp(arg, cl.players[i].name, MAX_SCOREBOARDNAME - 1))
			return i;
	}

	if (!arg[0])
		return PLAYER_NAME_NOMATCH;

	for (i = 0; arg[i]; i++)
	{
		if (!isdigit(arg[i]))
			return PLAYER_NAME_NOMATCH;
	}
	return ((slot = Player_IdtoSlot(Q_atoi(arg))) >= 0) ? slot : PLAYER_ID_NOMATCH;
}

int Player_NametoSlot(char *name)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && !strncmp(Info_ValueForKey(cl.players[i].userinfo, "name"), name, 31))
			return i;
	}
	return PLAYER_NAME_NOMATCH;
}

int Player_SlottoId (int slot)
{
	return (slot >= 0 && slot < MAX_CLIENTS && cl.players[slot].name[0]) ? cl.players[slot].userid : -1;
}

char *Player_MyName (void)
{
	return Info_ValueForKey(cls.demoplayback ? cls.userinfo : cl.players[cl.playernum].userinfo, "name");
}

int Player_GetSlot(char *arg)
{
	int response, i;

	if ( (response = Player_StringtoSlot(arg)) >= 0  || response == PLAYER_ID_NOMATCH )
		return response;

	if (arg[0] != '#')
		return response;

	for (i = 1; arg[i]; i++)
	{
		if (!isdigit(arg[i]))
			return PLAYER_NAME_NOMATCH;
	}

	if ((response = Player_NumtoSlot(Q_atoi(arg + 1))) >= 0)
		return response;

	return PLAYER_NUM_NOMATCH;
}

/********************************** String Utils ****************************************/

qboolean Util_F_Match(char *_msg, char *f_request)
{
	int offset, i, status, flags;
	char *s, *msg;

	msg = strdup(_msg);
	flags = TP_CategorizeMessage(msg, &offset);

	if (flags != 1 && flags != 4)
		return false;

	for (i = 0, s = msg + offset; i < strlen(s); i++)
		s[i] = s[i] & ~128;

	if (strstr(s, f_request) != s)
	{
		free(msg);
		return false;
	}
	status = 0;
	for (s += strlen(f_request); *s; s++)
	{
		if (isdigit(*s) && status <= 1)
		{
			status = 1;
		}
		else if (isspace(*s))
		{
			status = (status == 1) ? 2 : status;
		}
		else
		{
			free(msg);
			return false;
		}
	}
	free(msg);
	return true;
}

/********************************** TF Utils ****************************************/

static char *Utils_TF_ColorToTeam_Failsafe(int color)
{
	int i, j, teamcounts[8], numteamsseen = 0, best = -1;
	char *teams[MAX_CLIENTS];

	memset(teams, 0, sizeof(teams));
	memset(teamcounts, 0, sizeof(teamcounts));

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.players[i].name[0] || cl.players[i].spectator)
			continue;
		if (cl.players[i].real_bottomcolor != color)
			continue;
		for (j = 0; j < numteamsseen; j++)
		{
			if (!strcmp(cl.players[i].team, teams[j]))
				break;
		}
		if (j == numteamsseen)
		{
			teams[numteamsseen] = cl.players[i].team;
			teamcounts[numteamsseen] = 1;
			numteamsseen++;
		}
		else
		{
			teamcounts[j]++;
		}
	}
	for (i = 0; i < numteamsseen; i++)
	{
		if (best == -1 || teamcounts[i] > teamcounts[best])
			best = i;
	}
	return (best == -1) ? "" : teams[best];
}

char *Utils_TF_ColorToTeam(int color)
{
	char *s;

	switch (color)
	{
		case 13:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team1")) || *(s = Info_ValueForKey(cl.serverinfo, "t1")))
				return s;
			break;
		case 4:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team2")) || *(s = Info_ValueForKey(cl.serverinfo, "t2")))
				return s;
			break;
		case 12:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team3")) || *(s = Info_ValueForKey(cl.serverinfo, "t3")))
				return s;
			break;
		case 11:
			if (*(s = Info_ValueForKey(cl.serverinfo, "team4")) || *(s = Info_ValueForKey(cl.serverinfo, "t4")))
				return s;
			break;
		default:
			return "";
	}
	return Utils_TF_ColorToTeam_Failsafe(color);
}

int Utils_TF_TeamToColor(char *team)
{
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(13)))
		return 13;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(4)))
		return 4;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(12)))
		return 12;
	if (!Q_strcasecmp(team, Utils_TF_ColorToTeam(11)))
		return 11;
	return 0;
}

// maybe make this a macro?
static int is_valid_color_info (char *c)
{
	if ((*c >= '0' &&  *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z'))
		if ((*(c+1) >= '0' &&  *(c+1) <= '9') || (*(c+1) >= 'a' && *(c+1) <= 'z') || (*(c+1) >= 'A' && *(c+1) <= 'Z'))
			if ((*(c+2) >= '0' &&  *(c+2) <= '9') || (*(c+2) >= 'a' && *(c+2) <= 'z') || (*(c+2) >= 'A' && *(c+2) <= 'Z'))
				return 1;
	return 0;
}

char *Util_Remove_Colors(const char *string, int size)
{
	const char *src;
	char *dest, *new_string;
	int x = 0;
	int ignore = 0;

	new_string = malloc((size+1) * sizeof(char));

	if (new_string == NULL)
		return NULL;

	src = string;
	dest = new_string;

	while (*src != '\0' && x < size)
	{
		ignore = 0;
		if (*src == '&')
		{
			if (x + 1 < size)
			{
				if (*(src + 1) == 'c')
				{
					if (x + 5 >= size)
					{
						ignore = 0;
					}
					else
					{
						if (is_valid_color_info((src + 2)))
						{
							ignore = 1;
						}
					}
				}
			}
		}
		if (!ignore)
		{
			*dest = readablechars[(unsigned char)*src];
			src++;
			dest++;
			x++;
		}
		else
		{
			src += 5;
			x += 5;
		}
	}
	*dest = 0;

	return new_string;
}

// implementaiton taken from the vlc project
char *Util_strcasestr (const char *psz_big, const char *psz_little)
{
	char *p_pos = (char *)psz_big;

	if( !*psz_little ) return p_pos;

	while( *p_pos )
	{
		if( toupper( *p_pos ) == toupper( *psz_little ) )
		{
			char * psz_cur1 = p_pos + 1;
			char * psz_cur2 = (char *)psz_little + 1;
			while( *psz_cur1 && *psz_cur2 &&
			toupper( *psz_cur1 ) == toupper( *psz_cur2 ) )
			{
				psz_cur1++;
				psz_cur2++;
			}
			if( !*psz_cur2 ) return p_pos;
		}
		p_pos++;
	}
	return NULL;
}

/*
 * Directory Reading
 */

static void del_det_list(struct directory_entry_temp *list, int free_name)
{
	struct directory_entry_temp *temp, *temp1;

	temp = list;

	while (temp)
	{
		temp1 = temp->next;
		if (free_name)
			free(temp->name);
		free(temp);
		temp = temp1;
	}
}	

static struct directory_entry_temp *add_det(struct directory_entry_temp **list)
{
	struct directory_entry_temp *temp;

	if (*list == NULL)
	{
		temp = calloc(1, sizeof(struct directory_entry_temp));
		if (temp == NULL)
			return NULL;
		*list = temp;
		return temp;
	}
	temp = *list;
	while (temp->next)
		temp = temp->next;

	temp->next = calloc(1, sizeof(struct directory_entry_temp));

	return temp->next;
}

static int create_entries(struct directory_list *list, struct directory_entry_temp *det, int count)
{
	int i;
	struct directory_entry_temp *dett;

	list->entries = calloc(count, sizeof(struct directory_entry));
	if (list->entries == NULL)
		return 1;

	dett = det;
	for (i=0; i<count; i++)
	{
		if (dett == NULL)
			return 1;
		list->entries[i].type = dett->type;
		list->entries[i].name = dett->name;
		dett = dett->next;
	}

	return 0;
}

void Util_Dir_Delete(struct directory_list *dlist)
{
	int i;

	for (i=0; i<dlist->entry_count; i++)
		free(dlist->entries[i].name);

	free(dlist->base_dir);
	free(dlist->entries);
	free(dlist);
}

static int dir_entry_compare(const void *a, const void *b)
{
	struct directory_entry *x, *y;

	x = (struct directory_entry *) a;
	y = (struct directory_entry *) b;

	if (x->type == y->type)
	{
		return strcasecmp(x->name, y->name);
	}
	else if (x->type == et_file && y->type == et_dir)
		return 1;
	else if (x->type == et_dir && y->type == et_file)
		return -1;
	else
		return 0;
}

void Util_Dir_Sort(struct directory_list *dlist)
{
	qsort(dlist->entries, dlist->entry_count, sizeof(struct directory_entry), dir_entry_compare);
}

static int remove_dirs_from_det(struct directory_entry_temp **list)
{
	struct directory_entry_temp *tmp, *tmpn, *tmpp;
	int count;

	tmp = *list;
	tmpp = NULL;

	count = 0;

	while(tmp)
	{
		tmpn = tmp->next;
		if (tmp->type == et_dir)
		{
			if (tmp == *list)
				*list = tmpn;
			else if (tmpp)
				tmpp->next = tmpn;
			free(tmp->name);
			free(tmp);
			count++;
		}
		else
			tmpp = tmp;

		tmp = tmpn;
	}	

	return count;
}

static int filter_det(struct directory_entry_temp **list, char **filter)
{
	struct directory_entry_temp *tmp, *tmpn, *tmpp;
	char **cfilter;
	int remove, count;

	tmp = *list;
	tmpp = NULL;
	count = 0;

	while (tmp)
	{
		tmpn = tmp->next;
		cfilter = filter;
		remove = 1;
		if (tmp->type == et_dir)
		{
			remove = 0;
		}
		else
		{
			while (*cfilter)
			{
				if (Util_strcasestr(tmp->name, *cfilter) != NULL)
				{
					remove = 0;
					break;
				}
				cfilter++;
			}
		}

		if (remove)
		{
			count++;
			if (tmp == *list)
				*list = tmpn;
			else if (tmpp)
				tmpp->next = tmpn;
			free(tmp->name);
			free(tmp);
		}
		else
		{
			tmpp = tmp;
		}


		tmp = tmpn;
	}	

	return count;
}

struct directory_list *Util_Dir_Read(char *dir, int recursive, int remove_dirs, char **filters)
{
	struct directory_list *dlist;
	struct directory_entry_temp *det, *cdet;
	int count;

	if (dir == NULL)
		return NULL;

	dlist = calloc(1, sizeof(struct directory_list));
	if (dlist == NULL)
		return NULL;

	dlist->base_dir = strdup(dir);
	if (dlist->base_dir == NULL)
	{
		free(dlist);
		return NULL;
	}

	count = 0;
	det = NULL;

	if (Sys_Read_Dir(dir, NULL, &count, &det, &add_det))
	{
		del_det_list(det, 1);
		free(dlist->base_dir);
		free(dlist);
		return NULL;

	}

	if (recursive)
	{
		cdet = det;
		while (cdet)
		{
			if (cdet->type == et_dir)
			{
				if (Sys_Read_Dir(dir, cdet->name, &count, &det, &add_det))
				{
					del_det_list(det, 1);
                    free(det);
					free(dlist->base_dir);
					free(dlist);
					return NULL;
				}
			}
			cdet = cdet->next;
		}

		count -= remove_dirs_from_det(&det);
	}

	if (filters)
	{
		count -= filter_det(&det, filters);
	}

	if (create_entries(dlist, det, count))
	{
		del_det_list(det, 1);
        free(det);
		Util_Dir_Delete(dlist);
		return NULL;
	}

    del_det_list(det, 0);
    free(det);

	dlist->entry_count = count;

	return dlist;
}

int Colored_String_Length(char *string)
{
        char *ptr;
        int count = 0;
	int len;
	int ignore;

        ptr = string;
	len = strlen(string);

	ignore = 0;
        while (*ptr != '\0')
        {

                if (*ptr == '&')
                {
			if (count + 5 <= len)
			{
				if (*(ptr + 1) == 'c')
					if (is_valid_color_info((ptr+2)))
						ignore = 1;
			}
                }

		if (!ignore)
                {
                        ptr++;
                        count++;
                }
		else
		{
			ptr+=5;
			ignore = 0;
		}
        }

        return count;
}


