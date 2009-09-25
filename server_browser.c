/*
Copyright (C) 2009 Jürgen Legler

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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "quakedef.h"
#include "strl.h"
#include "keys.h"
#include "linked_list.h"
#include "serverscanner.h"
#include "readablechars.h"

void SB_AddMacros(void);

static int Color_For_Map (int m)
{
	m = bound(0, m, 13);
	return 16 * m + 8;
}

struct server
{
	struct linked_list_node node;
	char *ip;
	int port;
};

static struct ServerScanner *serverscanner;

cvar_t sb_masterserver = {"sb_masterserver", "asgaard.morphos-team.net:27000"};
cvar_t sb_player_drawing = {"sb_player_drawing", "0"};
cvar_t sb_refresh_on_activate = {"sb_refresh_on_activate", "1"};

char sb_macro_buf[512];

static int sb_open = 0;
static int sb_default_settings = 1;
static const struct QWServer **sb_qw_server;
static unsigned int sb_qw_server_count = 0;

static int sb_help_prev = -1;
static int sb_active_help_window = 0;
#define SB_HELP_WINDOWS 4

static int sb_check_serverscanner = 0;

// general display
static int sb_active_window= 0;
static int sb_selected_filter = 0;
static char sb_status_bar[512];

// server display
static int sb_server_insert;
static int sb_server_count_width = 1;

// filter display
static int sb_filter_insert;
static int sb_filter_edit;
static int sb_filter_delete;

// inserting filter
static int sb_filter_insert_selected_key;
static int sb_filter_insert_selected_type;
static char sb_filter_insert_value[512];
static int sb_filter_insert_value_position = 0;
static int sb_filter_insert_selected_box;

// player filter
static int sb_player_filter = 0;
static char sb_player_filter_entry[512];
static int sb_player_filter_entry_position = 0;
static double sb_player_filter_blink_time = 0;

static int sb_server_insert_selected_box;
static char sb_server_insert_ip[512];
static int sb_server_insert_ip_position;
static char sb_server_insert_port[512];
static int sb_server_insert_port_position;

#define SB_MAX_TABS 10
#define SB_SERVER 0
#define SB_FILTER 1
#define SB_HELP 2

#define SB_SORT_PLAYERS 0
#define SB_SORT_MAP 1
#define SB_SORT_HOSTNAME 2
#define SB_SORT_PING 3
#define SB_SORT_MAX 4

const struct QWServer *current_selected_server;

struct tab
{

	struct tab *prev, *next;
	
	char *name;
	int max_filter_keyword_length;
	int server_count;
	int player_count;
	int max_hostname_length;
	int max_map_length;
	int sb_position;
	int changed;
	int sort;
	int sort_dir;
	struct server **servers;
	int *server_index;
	char *player_filter;
	struct linked_list *filters;
};

static struct tab *tab_first;
static struct tab *tab_last;
static struct tab *tab_active;


static char *get_sort_name(struct tab *tab)
{
	if (tab->sort == SB_SORT_PLAYERS)
		return "players";
	else if (tab->sort == SB_SORT_PING)
		return "ping";
	else if (tab->sort == SB_SORT_MAP)
		return "map";
	else if (tab->sort == SB_SORT_HOSTNAME)
		return "hostname";
	else
		return "weird!";
}

struct filter
{
	struct linked_list_node node;
	int key;
	char *keyword;
	int type;
	char *value;
	float fvalue;
};

struct filter_types
{
	int type; // 0 - float, 1 - string
	char *name;
	char *description;
	int (*compare_function)(struct QWServer *server, struct filter *filter);
};

const char *filter_num_operators[] =
{
	"==",
	"<=",
	">="
};

const char *filter_char_operators[] =
{
	"isin",
	"!isin"
};

static int num_check(float sv, float fv, int type)
{
	if (type == 0)
	{
		if (sv == fv)
			return 1;
	}
	else if (type == 1)
	{
		if (sv <= fv)
			return 1;
	}
	else if (type == 2)
	{
		if (sv >= fv)
			return 1;
	}
	return 0;
}

static int char_check(const char *sv, char *fv, int type)
{
	if (type == 0)
	{
		if(strstr(fv, sv) == NULL)
			return 0;
		else
			return 1;
	}
	else if (type ==1)
	{
		if(strstr(sv, fv) == NULL)
			return 0;
		else
			return 1;
	}

	return 0;
}

static int filter_player_check(struct QWServer *server, struct filter *filter)
{
	return num_check(server->numplayers, filter->fvalue, filter->type);
}

static int filter_map_check(struct QWServer *server, struct filter *filter)
{
	if (server->map)
		return char_check(server->map, filter->value, filter->type);
	return 0;
}

static int filter_hostname_check(struct QWServer *server, struct filter *filter)
{
	if (server->hostname)
		return char_check(server->hostname, filter->value, filter->type);
	return 0;
}

static int filter_teamplay_check(struct QWServer *server, struct filter *filter)
{
	return num_check(server->teamplay, filter->fvalue, filter->type);
}

static int filter_max_clients_check(struct QWServer *server, struct filter *filter)
{
	return num_check(server->maxclients, filter->fvalue, filter->type);
}

static int filter_ping_check(struct QWServer *server, struct filter *filter)
{
	return num_check(server->pingtime/1000, filter->fvalue, filter->type);
}

#define SB_FILTER_TYPE_MAX 6

struct filter_types filter_types[SB_FILTER_TYPE_MAX] =
{
	{ 0, "players", "amount of players on the servers", filter_player_check},
	{ 1, "map", "mapname", filter_map_check},
	{ 1, "hostname", "hostname", filter_hostname_check},
	{ 0, "teamplay", "teamplay", filter_teamplay_check},
	{ 0, "max_clients", "max clients allowed on server", filter_max_clients_check},
	{ 0, "ping", "ping to the server", filter_ping_check}
};

static keydest_t old_keydest;

//static void SB_Filter_Delete_Handler(int ket);
void SB_Filter_Delete_Filter(void);
static void SB_Filter_Insert_Handler(int key);
static void SB_Server_Insert_Handler(int key);
static void update_tab(struct tab *tab);
 
static char *Filter_Type_String(int type)
{
	if (type == 0)
		return "==";
	else if (type == 1)
		return ">=";
	else if (type == 2)
		return "<=";
	else if (type == 3)
		return "isin";
	else if (type == 4)
		return "!isin";
	else
		return NULL;
}


static struct tab *sb_add_tab(char *name)
{
	struct tab *tab;

	if (name == NULL)
		return NULL;
	
	tab = calloc(1, sizeof(struct tab));

	if (tab == NULL)
		return NULL;


	tab->filters = List_Add(0, NULL, NULL);
	if (tab->filters == NULL)
	{
		free(tab);
		return NULL;
	}

	tab->name = strdup(name);
	if (tab->name == NULL)
	{
		free(tab->filters);
		free(tab);
		return NULL;
	}

	if (tab_first == NULL)
		tab_first = tab;
	
	if (tab_last == NULL)
	{
		tab_last = tab;
	}
	else
	{
		tab_last->next = tab;
		tab->prev = tab_last;
		tab_last = tab_last->next;
	}

	if (tab_active == NULL)
		tab_active = tab;


	return tab;
}


static void sb_del_tab(struct tab *tab)
{
	if (tab->next == NULL && tab->prev == NULL)
	{
		tab_first = tab_last = tab_active = NULL;
	}
	else if (tab->next && tab->prev == NULL)
	{
		tab_first = tab->next;
		tab->next->prev = NULL;
		if (tab == tab_active)
			tab_active = tab->next;
	}
	else if (tab->next == NULL && tab->prev)
	{
		tab->prev->next = NULL;
		tab_last = tab->prev;
		if (tab == tab_active)
			tab_active = tab->prev;
	}
	else
	{
		tab->prev->next = tab->next;
		tab->next->prev = tab->prev;
		if (tab == tab_active)
			tab_active = tab->next;
	}
	
	free(tab->name);
	free(tab->player_filter);
	free(tab->server_index);
	free(tab);
}

static void sb_del_tab_by_name(char *name)
{
	struct tab *tab;
	
	tab = tab_first;

	while(tab)
	{
		if (strcmp(tab->name, name) == 0)
		{
			sb_del_tab(tab);			
			return;
		}
		tab = tab->next;
	}

	Com_Printf("tab \"%s\" not found.\n", name);
}

static void sb_activate_tab(int num)
{
	struct tab *tab, *ptab;
	int i;
	
	tab = tab_first;
	i = 0;

	if (!tab)
		return;

	while (tab && i++ < num)
	{
		ptab = tab;
		tab = tab->next;
	}

	if (tab)
		tab_active = tab;
	else
		tab_active = ptab;
}

static void SB_Set_Statusbar (const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(sb_status_bar, sizeof(sb_status_bar), format, args);
	va_end(args);
}

static void SB_Refresh(void)
{
	struct tab *tab;

	ServerScanner_FreeServers(serverscanner, sb_qw_server);
	ServerScanner_Delete(serverscanner);
	serverscanner = NULL;
	sb_qw_server = NULL;
	sb_qw_server_count = 0;
	serverscanner = ServerScanner_Create(sb_masterserver.string);
	if (serverscanner == NULL)
		SB_Set_Statusbar("error creating server scanner!");

	tab = tab_first;
	while (tab)
	{
		tab->sb_position = 0;
		tab->changed = 0;
		tab = tab->next;
	}

}

static void handle_textbox(int key, char *string, int *position, int size)
{
	int i, length;
	length = strlen(string);
	if (key == K_LEFTARROW)
	{
		*position -= 1;	
		if (*position < 0)
			*position = 0;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		*position += 1;
		if (*position> length)
			*position = length;
		return;
	}

	if (key == K_DEL)
	{
		i = *position;
		if (*position >= length- 1)
			return;
		while(string[i] != '\0')
		{
			string[i] = string[i+1];				
			i++;
		}
		return;
	}

	if (key == K_BACKSPACE)
	{
		i = *position - 1;
		if (*position < 1)
			return;
		while(i<size)
		{
			string[i] = string[i+1];				
			i++;
		}
		*position -= 1;
		if (*position < 0)
			position = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;

	if (length == size) 
		return;
	i = length;
	while(i>*position)
	{
		string[i] = string[i-1];				
		i--;
	}

	string[*position] = key;
	*position += 1;
}

static int Check_Server_Against_Filter(struct tab *tab, const struct QWServer *server)
{
	struct filter *fe;

	fe = (struct filter *) List_Get_Node(tab->filters, 0);
	while (fe)
	{
		if (filter_types[fe->key].compare_function((struct QWServer *)server, fe) == 0)
			return 0;
		
		fe = (struct filter *)fe->node.next;
	}
	return 1;
}

static char *remove_colors(char *string, int size)
{
	char *ptr, *ptr1, *new_string;
	int x = 0;

	new_string = calloc(size+1, sizeof(char));

	if (new_string == NULL)
		return NULL;

	ptr = string;
	ptr1 = new_string;

	while (*ptr != '\0' && x < size)
	{
		if (*ptr == '&')
		{
			if (x + 1 < size)
			{
				if (*(ptr + 1) == 'c')
				{
					if (x + 5 >= size)
						break;
					ptr += 5;
					x += 5;
				}
			}
			else
			{
				break;
			}
		}
		*ptr1 = readablechars[(unsigned char)*ptr];

		ptr++;
		ptr1++;
		x++;
	}

	return new_string;
}

static int check_player(struct tab *tab, const struct QWServer *server)
{
	int i;
	char *player;
	char *player_uncolored;

	player = tab->player_filter;

	if (server->numplayers == 0 && server->numspectators == 0)
		return 0;

	for (i=0; i<server->numplayers; i++)
	{
		if (server->players[i].name)
		{

			player_uncolored = remove_colors(server->players[i].name , strlen(server->players[i].name));
			if (player_uncolored == NULL)
				continue;
			if (strcasestr(player_uncolored, player))
			{
				free(player_uncolored);
				return 1;
			}
			free(player_uncolored);
		}
	}

	for (i=0; i<server->numspectators; i++)
	{
		if (server->spectators[i].name)
		{
			if (strstr(server->spectators[i].name, player))
			{
				return 1;
			}
		}
	}
	return 0;
}

static int player_count_compare(const void *a, const void *b)
{
	struct QWServer *x, *y;
	x = (struct QWServer *) sb_qw_server[*(int *)a];
	y = (struct QWServer *) sb_qw_server[*(int *)b];

	if (y->numplayers == x->numplayers)
		return y->numspectators - x->numspectators;


	return (y->numplayers - x->numplayers);
}

static int ping_compare(const void *a, const void *b)
{
	struct QWServer *x, *y;

	x = (struct QWServer *) sb_qw_server[*(int *)a];
	y = (struct QWServer *) sb_qw_server[*(int *)b];

	if (x->status == QWSS_FAILED && y->status == QWSS_FAILED)
		return 0;
	else if (x->status == QWSS_FAILED)
		return 1;
	else if (y->status == QWSS_FAILED)
		return -1;

	return (x->pingtime - y->pingtime);
}

static int hostname_compare(const void *a, const void *b)
{
	struct QWServer *x, *y;
	char buf[64];
	x = (struct QWServer *) sb_qw_server[*(int *)a];
	y = (struct QWServer *) sb_qw_server[*(int *)b];

	if (x->hostname && y->hostname)
		return (strcasecmp(x->hostname, y->hostname));
	else if (!x->hostname && !y->hostname)
	{
		strlcpy(buf, NET_AdrToString(&x->addr), sizeof(buf));
		return (strcmp(buf, NET_AdrToString(&y->addr)));
	}
	else
	{
		if (x->hostname)
			return -1;
		else
			return 1;
	}
}

static int map_compare(const void *a, const void *b)
{
	struct QWServer *x, *y;
	x = (struct QWServer *) sb_qw_server[*(int *)a];
	y = (struct QWServer *) sb_qw_server[*(int *)b];

	if (x->map && y->map)
		return (strcasecmp(x->map, y->map));
	else
	{
		if (x->map)
			return -1;
		else
			return 1;
	}
}

int (* compare_functions[])(const void *a, const void *b) = 
{
	player_count_compare,
	map_compare,
	hostname_compare,
	ping_compare
};

static void sort_tab(struct tab *tab)
{
	qsort(tab->server_index, tab->server_count, sizeof(int), compare_functions[tab->sort]);
}

static int stubby (struct tab *tab, const struct QWServer *server)
{
	return 1;
}

static void update_tab(struct tab *tab)
{
	int count, i, x;
	int (*cf)(struct tab *tab, const struct QWServer *server);

	tab->max_hostname_length = 0;
	count = 0;

	if (List_Node_Count(tab->filters) && tab->player_filter == NULL)
		cf = Check_Server_Against_Filter;
	else if (tab->player_filter)
		cf = check_player;
	else
		cf = stubby;

	if (tab->server_index)
	{
		free(tab->server_index);
		tab->server_index = NULL;
	}

	for (count=0, x=0; x < sb_qw_server_count; x++)
	{
		if (cf(tab, sb_qw_server[x]))
			count++;
	}

	if (count > 10000 || count <= 0)
	{
		tab->server_count = 0;
		tab->sb_position = 0;
		return;
	}

	tab->server_index = calloc(count, sizeof(int));

	if (tab->server_index == NULL)
	{
		tab->server_count = 0;
		tab->sb_position = 0;
		return;
	}

	tab->server_count = count;

	for (x=0, i=0;x<sb_qw_server_count; x++)
	{
		if (cf(tab, sb_qw_server[x]))
				tab->server_index[i++] = x;		
	}

	if (tab->sb_position >= tab->server_count)
		tab->sb_position = tab->server_count - 1;
	if (tab->sb_position < 0)
		tab->sb_position = 0;

	sort_tab(tab);

	if (tab->changed == 0)
		return;

	if (tab == tab_active)
	{
		if (current_selected_server)
		{
			for (i=0; i< tab->server_count; i++)
				if (current_selected_server == sb_qw_server[tab->server_index[i]])
					break;
			if (i == tab->server_count)
			{
				return;
			}
			tab->sb_position = i;
		}
		else
		{
			i = tab->server_index[tab->sb_position];
			current_selected_server = sb_qw_server[i];
		}
	}
}

static void SB_Update_Tabs(void)
{
	struct tab *tab;

	tab = tab_first;

	while (tab)
	{
		update_tab(tab);
		tab = tab->next;
	}
}

static void SB_Help_Handler(int key)
{

	if (key == K_ESCAPE)
	{
		sb_active_window = sb_help_prev;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		sb_active_help_window++;
		if (sb_active_help_window >= SB_HELP_WINDOWS)
			sb_active_help_window = 0;
		return;
	}

	if (key == K_LEFTARROW)
	{
		sb_active_help_window--;
		if (sb_active_help_window <= 0)
			sb_active_help_window = SB_HELP_WINDOWS - 1;
		return;
	}
}

static void SB_Close()
{
	if (serverscanner && sb_refresh_on_activate.value == 1)
	{
		ServerScanner_FreeServers(serverscanner, sb_qw_server);
		ServerScanner_Delete(serverscanner);
		serverscanner = NULL;
		sb_qw_server = NULL;
		sb_qw_server_count = 0;
	}

	key_dest = old_keydest;
	sb_open = 0;
}

void SB_Key(int key)
{
	extern keydest_t key_dest;
	int i, update;
	const struct QWServer *server;
	struct tab *tab;
	char cmd[1024];
	char *kb;
	extern qboolean keyactive[256];

	if (key == 'h' && keydown[K_CTRL])
	{
		sb_help_prev = sb_active_window;
		sb_active_window = SB_HELP;
		return;
	}

	if (sb_active_window == SB_HELP)
	{
		SB_Help_Handler(key);
		return;
	}

	if (key == K_TAB)
	{
		if (keydown[K_CTRL])
		{
			if (sb_active_window == SB_SERVER)
			{
				sb_active_window = SB_FILTER;
			}
			else
			{
				sb_active_window = SB_SERVER;
			}
			return;
		}

		if (sb_active_window == SB_SERVER)
		{
			tab = tab_active;
			tab->sort++;
			if (tab->sort >= SB_SORT_MAX)
				tab->sort = 0;
			sort_tab(tab);
			SB_Set_Statusbar("Sorted by %s\n", get_sort_name(tab));
			return;
		}
	}

	if (sb_active_window == SB_FILTER)
	{
		if (sb_filter_delete == 1)
		{
			if (key == 'y')
			{
				SB_Filter_Delete_Filter();
				SB_Update_Tabs();
			}
			sb_filter_delete = 0;
			return;
		}

		if (sb_filter_insert == 1)
		{
			SB_Filter_Insert_Handler(key);
			return;
		}

		if (key == K_DOWNARROW)
		{
			sb_selected_filter++;
			if (sb_selected_filter >= List_Node_Count(tab_active->filters))
				sb_selected_filter = 0;
			return;
		}

		if (key == K_UPARROW)
		{
			sb_selected_filter--;		
			if (sb_selected_filter < 0)
				sb_selected_filter = List_Node_Count(tab_active->filters);
			return;
		}

		if (key == K_INS)
		{
			sb_filter_insert = 1;				
			return;
		}

		if (key == K_ENTER)
		{
			sb_filter_edit = 1;
			return;
		}

		if (key == K_DEL)
		{
			sb_filter_delete = 1;
			return;
		}
	}

	if (sb_active_window == SB_SERVER)
	{
		tab = tab_active;

		if (sb_player_filter == 1)
		{
			update = 0;
			if (key == K_ESCAPE)
			{
				sb_player_filter = 0;
				return;
			}

			if (key == K_ENTER)
				sb_player_filter = 0;

			handle_textbox(key, sb_player_filter_entry, &sb_player_filter_entry_position, sizeof(sb_player_filter_entry));
			if (tab->player_filter)
			{
				free(tab->player_filter);
				tab->player_filter = NULL;
				update = 1;
			}
			if (strlen(sb_player_filter_entry) > 0)
			{
				tab->player_filter = strdup(sb_player_filter_entry);
				if (tab->player_filter == NULL)
					Com_Printf("warning: strdup failed in \"%s\", line: %s.\n", __func__, __LINE__);
				update_tab(tab);
			}
			if (update)
				update_tab(tab);

			if (key != K_UPARROW && key != K_DOWNARROW && key != K_ENTER)
				return;
		}

		if (sb_server_insert == 1)
		{
			SB_Server_Insert_Handler(key);
			return;
		}

		if (key == K_INS)
		{
			sb_server_insert = 1;
		}

		if (key == K_UPARROW)
		{
			if (sb_qw_server == NULL)
				return;

			if (keydown[K_SHIFT])
				tab->sb_position -= 10;
			else if (keydown[K_CTRL])
				tab->sb_position -= 20;
			else
				tab->sb_position--;

			if (tab->sb_position < 0)
			{
				tab->sb_position = tab->server_count - 1;
				if (tab->sb_position < 0)
					tab->sb_position = 0;
			}

			if (tab->server_count == 0 || tab->sb_position > tab->server_count)
			{
				current_selected_server = NULL;
				return;
			}

			i = tab->server_index[tab->sb_position];
			current_selected_server = sb_qw_server[i];
			tab->changed = 1;
			return;
		}

		if (key == K_DOWNARROW)
		{
			if (sb_qw_server == NULL)
				return;

			if (keydown[K_SHIFT])
				tab->sb_position += 10;
			else if (keydown[K_CTRL])
				tab->sb_position += 20;
			else
				tab->sb_position++;
			if (tab->sb_position >= tab->server_count)
				tab->sb_position = 0;

			if (tab->server_count == 0 || tab->sb_position > tab->server_count)
			{
				current_selected_server = NULL;
				return;
			}

			i = tab->server_index[tab->sb_position];
			current_selected_server = sb_qw_server[i];
			tab->changed = 1;
			return;
		}

		if (key == K_ENTER)
		{
			if (sb_qw_server == NULL)
				return; 

			if (keydown[K_CTRL])
			{
				Cbuf_AddText("spectator 1\n");

				if (tab->server_index)
				{
					i = tab->server_index[tab->sb_position];
					server = sb_qw_server[i];
				}
				else
				{
					server = sb_qw_server[tab->sb_position];
				}
				Cbuf_AddText(va("connect %s\n", NET_AdrToString(&server->addr)));
				SB_Close();
				return;
			}


			if (tab->server_index)
			{
				i = tab->server_index[tab->sb_position];
				server = sb_qw_server[i];
			}
			else
			{
				server = sb_qw_server[tab->sb_position];
			}


			if (server->maxclients == server->numplayers)
				Cbuf_AddText("spectator 1\n");
			else
				Cbuf_AddText("spectator 0\n");

			Cbuf_AddText(va("connect %s\n", NET_AdrToString(&server->addr)));
			SB_Close();
			return;
		}

		if (key == 'f' && keydown[K_CTRL])
		{
			sb_player_filter = 1;
			return;
		}

		if (key == 'r')
		{
			if (keydown[K_CTRL])
			{
				SB_Refresh();
				return;
			}
			
			if (sb_qw_server)
			{
				if (tab->server_index)
				{
					i = tab->server_index[tab->sb_position];
					server = sb_qw_server[i];
				}
				else
				{
					server = sb_qw_server[tab->sb_position];
				}
			}

			ServerScanner_RescanServer(serverscanner, server);
		}
	}

	if (key == K_ESCAPE)
	{
		SB_Close();
		return;
	}

	if (key == K_LEFTARROW)
	{
		if (tab_active->prev)
			tab_active = tab_active->prev;
		else
			tab_active = tab_last;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		if (tab_active->next)
			tab_active = tab_active->next;
		else
			tab_active = tab_first;

		return;
	}

	switch (key)
	{
		case '1':
			sb_activate_tab(0);
			break;
		case '2':
			sb_activate_tab(1);
			break;
		case '3':
			sb_activate_tab(2);
			break;
		case '4':
			sb_activate_tab(3);
			break;
		case '5':
			sb_activate_tab(4);
			break;
		case '6':
			sb_activate_tab(5);
			break;
		case '7':
			sb_activate_tab(6);
			break;
		case '8':
			sb_activate_tab(7);
			break;
		case '9':
			sb_activate_tab(8);
			break;
		case '0':
			sb_activate_tab(9);
			break;
	}

	if (key >= K_F1 && key <= K_F12)
	{
		kb = keybindings[key];
		if (kb) {
			if (kb[0] == '+'){	// button commands add keynum as a parm
				snprintf (cmd, sizeof(cmd), "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
				keyactive[key] = true;
			} else {
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}

	}
}

static void SB_Server_Add(char *ip, int port)
{
	/*
	struct server *entry;

	entry = List_Get_Node(server, 0);

	while (entry)
	{
		if (strcmp(ip, entry->ip) == 0)
			if (port == entry->port)
				return;
		entry = (struct server *)entry->node.next;
	}

	entry = calloc(1, sizeof(struct server));
	if (!entry)
		return;
	
	entry->ip = strdup(ip);
	entry->port = port;
	List_Add_Node(server, entry);

	sb_server_count++;
	SB_Update_Tabs();
	*/
}

static void SB_Add_Filter_To_Tab(struct tab *tab, int key , int type, char *value)
{
	struct filter *f;
	int i;

	if (!tab)
		return;
	
	if (strlen(value) == 0)
		return;

	f = calloc(1, sizeof(struct filter));
	if (!f)
		return;

	f->key = key;

	f->keyword = strdup(filter_types[key].name);

	if (f->keyword == NULL)
	{
		Com_Printf("error: strdup failed in \"%s\", line: %s\n.", __func__, __LINE__);
		free(f);
		return;
	}	

	f->type = type;

	f->value = strdup(value);
	if (f->value == NULL)
	{
		Com_Printf("error: strdup failed in \"%s\", line: %s\n.", __func__, __LINE__);
		free(f->keyword);
		free(f);
		return;
	}	

	if (filter_types[key].type == 0)
		f->fvalue = atof(value);

	List_Add_Node(tab->filters, f);
	i = strlen(f->keyword);
	if (tab->max_filter_keyword_length < i)
		tab->max_filter_keyword_length = i;
}

static void sb_default_tabs(void)
{
	struct tab *tab;

	tab = sb_add_tab("all");
	tab = sb_add_tab("duel");
	SB_Add_Filter_To_Tab(tab, 0, 2, "1");
	SB_Add_Filter_To_Tab(tab, 4, 0, "2");
	tab = sb_add_tab("2on2");
	SB_Add_Filter_To_Tab(tab, 0, 2, "1");
	SB_Add_Filter_To_Tab(tab, 4, 0, "4");
	tab = sb_add_tab("4on4");
	SB_Add_Filter_To_Tab(tab, 0, 2, "1");
	SB_Add_Filter_To_Tab(tab, 4, 0, "8");
}

void SB_Activate_f(void)
{
	extern keydest_t key_dest;
	
	old_keydest = key_dest;
	key_dest = key_serverbrowser;
	sb_open = 1;

	if (tab_first == NULL)
	{
		sb_default_tabs();
		tab_active = tab_first;
	}
	

	if (serverscanner && sb_refresh_on_activate.value == 1)
	{
		SB_Refresh();
	}

	if (serverscanner == NULL)
	{
		serverscanner = ServerScanner_Create(sb_masterserver.string);
		if (serverscanner == NULL)
			SB_Set_Statusbar("error creating server scanner!");
	}
	SB_Update_Tabs();
}



static int check_selected_type(int type)
{
	int max;	

	if (filter_types[sb_filter_insert_selected_key].type == 0)
		max = 3;
	else
		max = 2;

	if (type < 0)
		type = max - 1;
	if (type >= max )
		type = 0;

	return type;
}

static void SB_Filter_Insert_Handler(int key)
{
	if (key == K_ENTER)
	{
		SB_Add_Filter_To_Tab(tab_active, sb_filter_insert_selected_key, sb_filter_insert_selected_type, sb_filter_insert_value);
		sb_filter_insert = 0;
		sb_filter_insert_value_position = 0;
		update_tab(tab_active);
		sb_default_settings = 0;
		return;
	}

	if (key == K_TAB)
	{	
		sb_filter_insert_selected_box++;
		if (sb_filter_insert_selected_box > 2)
			sb_filter_insert_selected_box = 0;

		return;
	}

	if (key == K_ESCAPE)
	{
		sb_filter_insert = 0;
		return;
	}

	if (sb_filter_insert_selected_box== 1)
	{
		if (key == K_DOWNARROW)
			sb_filter_insert_selected_type--;
		else if (key == K_UPARROW)
			sb_filter_insert_selected_type++;

		sb_filter_insert_selected_type = check_selected_type(sb_filter_insert_selected_type);
		return;
	}

	if (sb_filter_insert_selected_box == 2)
	{
		handle_textbox(key, sb_filter_insert_value, &sb_filter_insert_value_position, sizeof(sb_filter_insert_value));
		return;
	}
	
	if (sb_filter_insert_selected_box == 0)
	{
		if (key == K_DOWNARROW)
			sb_filter_insert_selected_key--;
		else if (key == K_UPARROW)
			sb_filter_insert_selected_key++;

		if (sb_filter_insert_selected_key < 0)
			sb_filter_insert_selected_key = SB_FILTER_TYPE_MAX - 1;
		if (sb_filter_insert_selected_key >= SB_FILTER_TYPE_MAX)
			sb_filter_insert_selected_key = 0;

		sb_filter_insert_selected_type = check_selected_type(sb_filter_insert_selected_type);
		return;
	}
}

void SB_Filter_Delete_Filter(void)
{
	List_Remove_Node(tab_active->filters, sb_selected_filter, 1);
}

static void SB_Server_Insert_Handler(int key)
{
	if (key == K_TAB)
	{
		sb_server_insert_selected_box++;
		if (sb_server_insert_selected_box > 1)
			sb_server_insert_selected_box = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		sb_server_insert = 0;
		return;
	}

	if (key == K_ENTER)
	{
		SB_Server_Add(sb_server_insert_ip, atoi(sb_server_insert_port));
		sb_server_insert = 0;
		return;
	}

	if (sb_server_insert_selected_box == 0)
	{
		handle_textbox(key, sb_server_insert_ip, &sb_server_insert_ip_position, sizeof(sb_server_insert_ip));
		return;
	}

	if (sb_server_insert_selected_box == 1)
	{
		handle_textbox(key, sb_server_insert_port, &sb_server_insert_port_position, sizeof(sb_server_insert_port));
		return;
	}
}

static void SB_Draw_Tabs(void)
{
	int width, x, l, r, i;
	struct tab *tab;

	width = vid.conwidth/8;


	x = strlen(tab_active->name);
	if (x % 2)
		x++;

	l = x;
	x = x/ 2 + 1;

	Draw_ColoredString((width/2 - x) * 8, 0, va("&cf44<%-*s>",l ,tab_active->name), 1);
	l = width/2 - x - 1;
	r = width/2 + x + 1;

	tab = tab_active->prev;
	i = l;
	while (tab)
	{
		x = strlen(tab->name);
		i -= x;
		Draw_String(i*8, 0, tab->name);
		i--;
		tab = tab->prev;
	}

	tab = tab_active->next;
	i = r;
	while (tab)
	{
		x = strlen(tab->name);
		Draw_String(i*8, 0, tab->name);
		i += x;
		i++;
		tab = tab->next;
	}

	

}

static void SB_Draw_Background(void)
{
	Draw_Fill(0, 0, vid.conwidth, vid.conheight, 1);
}

static void SB_Draw_Filter_Insert(void)
{
	int i;

	i = 0;
	Draw_String(8,24, filter_types[sb_filter_insert_selected_key].name);
	Draw_String(0, 24 + sb_filter_insert_selected_box * 8, ">");
	Draw_String(8, 48 + 8 * i++, "press arrow up/down to switch trough available options");
	Draw_String(8, 48 + 8 * i++, "tab to switch trough the entry fields");
	Draw_String(8, 48 + 8 * i++, "esc to leave");

	if (sb_filter_insert_selected_box == 0)
	{
		Draw_String(8, 48 + 8 * i++, filter_types[sb_filter_insert_selected_key].description);
	}

	if (sb_filter_insert_selected_box == 1)
	{
		if (filter_types[sb_filter_insert_selected_key].type == 0)
		{
			Draw_String(8, 48 + 8 * i++, (char *)filter_num_operators[sb_filter_insert_selected_type]);
		}
		else
		{
			Draw_String(8, 48 + 8 * i++, (char *)filter_num_operators[sb_filter_insert_selected_type]);
		}
	}

	if (filter_types[sb_filter_insert_selected_key].type == 0)
		Draw_String(8, 32, (char *)filter_num_operators[sb_filter_insert_selected_type]);
	else
		Draw_String(8, 32, (char *)filter_char_operators[sb_filter_insert_selected_type]);

	Draw_String(8, 40, sb_filter_insert_value);
	if (sb_filter_insert_selected_box == 2)
	{
		Draw_String(8 + 8*sb_filter_insert_value_position, 40, "_");
	}
}

static void SB_Draw_Filter(void)
{
	int i;
	struct filter *filter;
	
	if (sb_filter_insert)
	{
		SB_Draw_Filter_Insert();
		return;
	}

	Draw_String(8, 16, "Filter:");
	filter = List_Get_Node(tab_active->filters, 0);
	i = 0;
	while(filter)
	{
		if (i == sb_selected_filter)
		{
			Draw_String(0, 24+i*8,">");
		}
		Draw_String(8, 24 + i++ * 8, va("%*s %5s %s", tab_active->max_filter_keyword_length, filter->keyword, Filter_Type_String(filter->type), filter->value)); 
		filter = (struct filter *)filter->node.next;
	}
}

static void SB_Draw_Server_Insert(void)
{
	Draw_String(8, 24, va("ip: %s", sb_server_insert_ip));
	if (sb_server_insert_selected_box == 0)
		Draw_String(8 + 8* (sb_server_insert_ip_position + 3), 24, "_");

	Draw_String(8, 32, va("port: %s", sb_server_insert_port));
	if (sb_server_insert_selected_box == 1)
		Draw_String(8 + 8* (sb_server_insert_port_position + 6), 32, "_");
}

//static int sort_playrers(const coi
static int sort_players_team(const void *a, const void *b)
{
	struct QWPlayer *x, *y;
	int i;

	x = *(struct QWPlayer **)a;
	y = *(struct QWPlayer **)b;
	
	if (x->team && y->team)
	{
		if ((i = strcmp(x->team ,y->team)) == 0)
			return (y->frags - x->frags);
		else 
			return i;
	}
	
	return (y->frags - x->frags);
}

static int sort_players(const void *a, const void *b)
{
	struct QWPlayer *x, *y;

	x = *(struct QWPlayer **)a;
	y = *(struct QWPlayer **)b;
	
	return (y->frags - x->frags);
}

static void SB_Draw_Server(void)
{
	int i, x, y, z;
	int line_space, player_space;
	int offset;
	struct tab *tab;
	const char *hostname, *map;
	char string[512];
	int position;
	const struct QWPlayer **sorted_players;
	const struct QWServer *server;
	const struct QWPlayer *player;
	const struct QWSpectator *spectator;

	if (sb_server_insert)
	{
		SB_Draw_Server_Insert();
		return;
	}

	if (sb_qw_server_count == 0)
		return;

	tab = tab_active;
	if (!tab)
		return;
	player_space = 0;

	if (sb_player_filter == 1 || tab->player_filter)
	{
		snprintf(string, 512, "Server: %*i/%*i - player_filter: ", sb_server_count_width, tab->sb_position + 1, sb_server_count_width, tab->server_count);
		i = strlen(string);
		Draw_String(8, 16,string);
		x = 1;
		if (tab->player_filter)
		{
			x = strlen(tab->player_filter);
			if (x == 0)
				x = 1;
			else 
				x += 1;

			if (sb_player_filter == 1)
				Draw_Fill(8 + i *8, 16, x * 8, 8, 55);
			Draw_String(8 + i *8, 16, tab->player_filter);
		}
		else
		{
			if (sb_player_filter == 1)
			Draw_Fill(8 + i *8, 16, x * 8, 8, 55);
		}

		if (sb_player_filter == 1)
		{
			if (sb_player_filter_blink_time < cls.realtime)
				Draw_Character(8 + i *8 + sb_player_filter_entry_position *8, 16, 11);

			if (sb_player_filter_blink_time + 0.2f < cls.realtime)
				sb_player_filter_blink_time = cls.realtime + 0.2f;
		}
	}
	else
		Draw_String(8, 16,va("Server: %*i/%*i", sb_server_count_width, tab->sb_position + 1, sb_server_count_width, tab->server_count));

	if (tab->server_count == 0)
		return;

	line_space = vid.conheight/8 - 4;
	if (sb_player_drawing.value == 0)
	{
		if (tab->server_index == NULL)
		{
			z = tab->sb_position;
		}
		else 
		{
			z = tab->server_index[tab->sb_position];
		}

		if (sb_qw_server[z]->numplayers || sb_qw_server[z]->numspectators)
		{
			player_space = sb_qw_server[z]->numspectators + sb_qw_server[z]->numplayers + 1;
			if (player_space > line_space/2)
				line_space -= player_space;
			z = 0;
		}
		else
			z = 0;
	}

	x = 0;
	if (tab->server_count > line_space)
	{
		x = tab->sb_position - ((float)line_space/2.0f);
		if (x<0)
		{
			x = 0;
			offset = tab->sb_position;
			position = 2;
		}
		else if (x> tab->server_count - line_space)
		{
			x = tab->server_count - line_space;
			offset = line_space - (tab->server_count - tab->sb_position);
			position = 0;
		}
		else
		{
			offset = tab->sb_position - x;
			position = 1;
		}
	}
	else
	{
		offset = tab->sb_position;
		position = 3;
	}

	offset += z;
	Draw_Fill(0,(offset+3) * 8, vid.conwidth , 9 , 13);
	y = x+1;

	for (i=0;x<tab->server_count && i < line_space ;i++,x++)
	{
		if (tab->server_index == NULL)
		{
			z = x;
		}
		else 
		{
			z = tab->server_index[x];
		}
		server = sb_qw_server[z];
		
		if (server->hostname == NULL)
			hostname = NET_AdrToString(&server->addr);
		else
			hostname = server->hostname;

		if (server->map == NULL)
			map = " ";
		else
			map = server->map;

		snprintf(string, sizeof(string),"%*i: %3i %2i/%2i %-*.*s %s", sb_server_count_width, y, server->pingtime/1000, server->numplayers, server->maxclients, 8, 8, map, hostname);
		Draw_String(8, 24 + i * 8, string);
		y++;
	}
	Draw_String(0,24 + offset * 8,">");

	if (sb_qw_server_count == 0)
		return;
	
	if (tab->sb_position < 0 || tab->sb_position >= tab->server_count)
		return;

	if (tab->server_index == NULL)
	{
		server = sb_qw_server[tab->sb_position];
	}
	else
	{
		z = tab->server_index[tab->sb_position];
		server = sb_qw_server[z];
	}

	if (tab->server_index != NULL)
	{
		x = tab->sb_position;
		x = tab->server_index[x];
		if (x>sb_qw_server_count)
			return;
		server = sb_qw_server[x];
	}
	else 
	{
		if (tab->sb_position > sb_qw_server_count)
			return;
		server = sb_qw_server[tab->sb_position];
	}

	if (!server)
		return;

	if (sb_player_drawing.value == 0)
	{
		if (server->numplayers == 0 && server->numspectators == 0)
			return;
		y = (line_space - player_space) + 3;
		z = vid.conwidth/8;

		if (player_space < line_space/2)
			Draw_Fill(0, y*8, vid.conwidth, player_space * 8, 2);
		Draw_Fill(0, y*8, vid.conwidth, 8, 20);
		Draw_String(0, (y++)*8, "ping time frags team");
		player = (struct QWPlayer *)&server->players[0];

		sorted_players = calloc(server->numplayers, sizeof(struct QWPlayer **));
		if (sorted_players == NULL)
			return;

		for(i = 0;i<server->numplayers;i++)
		{
			sorted_players[i] = &server->players[i];
		}

		//qsort(tab->server_index, tab->server_count, sizeof(int), player_count_compare);
		if (server->teamplay > 0)
			qsort(sorted_players, server->numplayers, sizeof(struct QWPlayer *), sort_players_team);
		else
			qsort(sorted_players, server->numplayers, sizeof(struct QWPlayer *), sort_players);

		for(i = 0;i<server->numplayers;i++)
		{
			player = sorted_players[i];
			if (player->name)
			{
				Draw_Fill((5+5)*8, y*8, 5*8, 4, Color_For_Map(player->topcolor));
				Draw_Fill((5+5)*8, y*8+4, 5*8, 4, Color_For_Map(player->bottomcolor));
				if (player->team)
					Draw_ColoredString(0, y++ * 8, va("%4i %4i %4i  %4.4s&cFFF  %s", player->ping, player->time, player->frags, player->team, player->name), 0);
				else
					Draw_ColoredString(0, y++ * 8, va("%4i %4i %4i  %4.4s&cFFF  %s", player->ping, player->time, player->frags, " ", player->name), 0);
			}
		}
		
		free(sorted_players);

		spectator = &server->spectators[0];
		for(i = 0;i<server->numspectators;i++)
		{
			if (spectator->name)
				Draw_ColoredString(0, y++ * 8, va("%4i %4i &cF20s&cF50p&cF80e&c883c&cA85t&c668a&c55At&c33Bo&c22Dr&cFFF   %s", -spectator->ping, -spectator->time, spectator->name), 0);
			spectator++;
		}
	}
}

static void SB_Draw_Status_Bar(void)
{
	Draw_String(0, vid.conheight - 8, sb_status_bar);
}

static void SB_Draw_Help(void)
{
	int i = 0;

	Draw_String(8, 0 + 8 * i++, "press arrow left/right to switch help screens");
	Draw_String(8, 0 + 8 * i++, "esc to quit");
	if (sb_active_help_window == 0)
	{
		Draw_String(8, 8 + 8 * i++, "general controls:");
		Draw_String(8, 8 + 8 * i++, " 1->0, arrow left/right -  to switch tabs");
		Draw_String(8, 8 + 8 * i++, " pgup/pgdown, arrow up/down - to scroll");
		Draw_String(8, 8 + 8 * i++, " esc - will quit submenus, browser");
		
	}
	else if (sb_active_help_window == 1)
	{
		Draw_String(8, 8 + 8 * i++, "server screen:");
		Draw_String(8, 8 + 8 * i++, " ctrl + r - rescan all server");
		Draw_String(8, 8 + 8 * i++, " r - rescan selected server");
		Draw_String(8, 8 + 8 * i++, " enter - to join as player");
		Draw_String(8, 8 + 8 * i++, " ctrl + enter - to join as spectator");
		Draw_String(8, 8 + 8 * i++, " enter - to join as player");
		Draw_String(8, 8 + 8 * i++, " ctrl + f - start player search");
		Draw_String(8, 8 + 8 * i++, " tab - to switch through sort mode");
	}
	else if (sb_active_help_window == 2)
	{
		Draw_String(8, 8 + 8 * i++, "filter screen:");
		Draw_String(8, 8 + 8 * i++, " insert - to add filter");
	}
	else if (sb_active_help_window == 3)
	{
		Draw_String(8, 8 + 8 * i++, "insert filter screen:");
		Draw_String(8, 8 + 8 * i++, " tab - to switch through entry boxes");
		Draw_String(8, 8 + 8 * i++, " arrow up/down - to select compare type");
	}
}

void SB_Frame(void)
{
	enum ServerScannerStatus sss;
	int count,todo;
	int x;

	if (!sb_open)
		return;

	if (serverscanner)
		ServerScanner_DoStuff(serverscanner);

	if (serverscanner && sb_check_serverscanner)
	{
		sss = ServerScanner_GetStatus(serverscanner);
		if (sss == SSS_IDLE)
		{
			SB_Set_Statusbar("All done. Press \"ctrl + h\" for help.");
			sb_check_serverscanner = 0;
		}
		else if (sss == SSS_SCANNING)
		{
			todo = 0;//ServerScanner_ServersToScan(serverscanner);			
			count = 0;//ServerScanner_Servers(serverscanner);
			SB_Set_Statusbar("Scanning servers. Press \"ctrl + h\" for help");
		}
		else if (sss == SSS_PINGING)
		{
			SB_Set_Statusbar("Pinging servers. Press \"ctrl + h\" for help.");
		}
	}

	if (serverscanner)
	{
		if(ServerScanner_DataUpdated(serverscanner))
		{
			if (sb_qw_server)
				ServerScanner_FreeServers(serverscanner, sb_qw_server);
			sb_qw_server = ServerScanner_GetServers(serverscanner, &sb_qw_server_count);
			sb_server_count_width = 1;
			x = sb_qw_server_count;
			while((x/=10)) sb_server_count_width++;
			SB_Update_Tabs();
		}
	}

	SB_Draw_Background();
	if (sb_active_window != SB_HELP)
		SB_Draw_Tabs();
	if (sb_active_window == SB_SERVER && serverscanner)
		SB_Draw_Server();
	if (sb_active_window == SB_FILTER)
		SB_Draw_Filter();
	if (sb_active_window == SB_HELP)
		SB_Draw_Help();
	SB_Draw_Status_Bar();
	sb_check_serverscanner = 1;
}

void SB_Quit(void)
{
	if (serverscanner == NULL)
		return;
	ServerScanner_FreeServers(serverscanner, sb_qw_server);
	ServerScanner_Delete(serverscanner);
}

static void SB_List_Tabs_f(void)
{
	struct tab *tab;

	tab = tab_first;

	while(tab)
	{
		Com_Printf("%s\n", tab->name);
		tab = tab->next;
	}
}

static void SB_Add_Tab_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s [tab name]\n", Cmd_Argv(0));
	}

	sb_default_settings = 0;
	sb_add_tab(Cmd_Argv(1));
}

static void SB_Del_Tab_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s [tab name]\n", Cmd_Argv(0));
	}

	sb_del_tab_by_name(Cmd_Argv(1));
}

static void SB_Add_Filter_f(void)
{
	struct tab *tab;
	int filter, operator, i, x;
	char **operators;

	if (Cmd_Argc() != 5)
	{
		Com_Printf("Usage: %s [tab name] [filter name] [filter operator] [value]\n", Cmd_Argv(0));
	}

	tab = tab_first;

	while (tab)
	{
		if (strcmp(tab->name, Cmd_Argv(1)) == 0)
			break;
		tab = tab->next;
	}

	if (tab == NULL)
	{
		Com_Printf("%s: tab %s not found.\n", Cmd_Argv(0), Cmd_Argv(1));
		return;
	}

	filter = -1;

	for (i = 0; i < SB_FILTER_TYPE_MAX; i++)
	{
		if (strcmp(filter_types[i].name, Cmd_Argv(2)) == 0)
		{
			filter = i;
			break;
		}
	}

	if (filter == -1)
	{
		Com_Printf("%s: filter type %s not found.\n", Cmd_Argv(0), Cmd_Argv(2));
		Com_Printf("use sb_list_filter_types to get a list of available filters.\n", Cmd_Argv(0), Cmd_Argv(2));
		return;
	}

	operator = -1;

	if (filter_types[filter].type == 0)
	{
		x = 3;
		operators = filter_num_operators;
	}
	else
	{
		x = 2;
		operators = filter_char_operators;
	}

	for (i=0; i<x; i++)
	{
		if (strcmp(operators[i], Cmd_Argv(3)) == 0)
		{
			operator = i;
			break;
		}
	}

	if (operator == -1)
	{
		Com_Printf("%s: filter operator %s not found.\n", Cmd_Argv(0), Cmd_Argv(3));
		Com_Printf("use sb_list_filter_types to get a list of available filters and operators.\n", Cmd_Argv(0), Cmd_Argv(2));
		return;
	}

	sb_default_settings = 0;

	SB_Add_Filter_To_Tab(tab, filter, operator, Cmd_Argv(4));
}


void Dump_SB_Config(FILE *f);
static void SB_Write_Config(void)
{
	FILE *f;
	char *file;

	file = va("%s/qw/%s.cfg", com_basedir, Cmd_Argv(1));

	f = fopen(file, "wb");

	Dump_SB_Config(f);

	fclose(f);
}

static void SB_Set_Clipboard_f(void)
{
	char buf[512];
	char *s;
	int i;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s [clip bord text]\n", Cmd_Argv(0));
	}

	for (i=1; i<Cmd_Argc();i++)
	{
		strlcat(buf, Cmd_Argv(i), sizeof(buf));
		strlcat(buf, " ", sizeof(buf));
	}

	s = buf;

	while (*s)
	{
		*s = readablechars[(unsigned char) *s] & 127;	
		s++;
	}

	VID_SetClipboardText(buf);

}

void SB_Init(void)
{
	Cmd_AddCommand("sb_activate", &SB_Activate_f);
	Cmd_AddCommand("sb_list", &SB_List_Tabs_f);
	Cmd_AddCommand("sb_add_tab", &SB_Add_Tab_f);
	Cmd_AddCommand("sb_del_tab", &SB_Del_Tab_f);
	Cmd_AddCommand("sb_add_filter", &SB_Add_Filter_f);
	Cmd_AddCommand("sb_write_config", &SB_Write_Config);
	Cmd_AddCommand("sb_set_clipboard", &SB_Set_Clipboard_f);
	SB_Set_Statusbar("just started!. press \"ctrl + h\" for help\n");
	SB_AddMacros();
}

void SB_CvarInit(void)
{
	Cvar_Register(&sb_masterserver);
	Cvar_Register(&sb_player_drawing);
	Cvar_Register(&sb_refresh_on_activate);
}


void Dump_SB_Config(FILE *f)
{
	struct tab *tab;
	struct filter *filter;

	if (f == NULL)
		return;

	if (sb_default_settings == 1)
		return;

	tab = tab_first;

	while(tab)
	{
		fprintf(f, va("sb_add_tab %s\n", tab->name));

		filter = List_Get_Node(tab->filters, 0);

		while (filter)
		{
			if (filter_types[filter->key].type == 0)
				fprintf(f, va("sb_add_filter %s %s %s %f\n", tab->name, filter_types[filter->key].name, filter_num_operators[filter->type], filter->fvalue));
			else
				fprintf(f, va("sb_add_filter %s %s %s %s\n", tab->name, filter_types[filter->key].name, filter_char_operators[filter->type], filter->value));
			filter = (struct filter *)filter->node.next;
		}

		tab = tab->next;
	}
}

char *SB_Macro_Ip(void)
{
	if (current_selected_server)
		return NET_AdrToString(&current_selected_server->addr);
	else
		return "none";
}

char *SB_Macro_Hostname(void)
{
	if (current_selected_server)
	{
		if(current_selected_server->hostname)
			return current_selected_server->hostname;
		else
			return "none";
	}
	else
		return "none";
}

char *SB_Macro_Map(void)
{
	if (current_selected_server)
	{
		if(current_selected_server->map)
			return current_selected_server->map;
		else
			return "none";
	}
	else
		return "none";
}

char *SB_Macro_Player(void)
{
	if (current_selected_server)
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", current_selected_server->numplayers);
	else
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", -1);
	return sb_macro_buf;
}

char *SB_Macro_Max_Player(void)
{
	if (current_selected_server)
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", current_selected_server->maxclients);
	else
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", -1);
	return sb_macro_buf;
}

char *SB_Macro_Ping(void)
{
	if (current_selected_server)
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", current_selected_server->pingtime / 1000);
	else
		snprintf(sb_macro_buf, sizeof(sb_macro_buf), "%i", -1);
	return sb_macro_buf;
}

char *SB_Macro_Player_Names(void)
{
	struct QWPlayer *player;
	int i;

	if (current_selected_server)
	{
		if (current_selected_server->numplayers > 0)
		{
			sb_macro_buf[0] = '\0';
			for (i=0, player=current_selected_server->players; i<current_selected_server->numplayers && player;i++)
			{
				if (i != 0)
					strlcat(sb_macro_buf, " - ", sizeof(sb_macro_buf));	
				if (player->name)
				{
					strlcat(sb_macro_buf, player->name, sizeof(sb_macro_buf));	
				}
				player++;
			}
			return sb_macro_buf;
		}
		else
			return "none";
	}
	else
		return "none";
}

void SB_AddMacros(void)
{
	Cmd_AddMacro("sb_player_names", SB_Macro_Player_Names);
	Cmd_AddMacro("sb_max_player", SB_Macro_Max_Player);
	Cmd_AddMacro("sb_hostname", SB_Macro_Hostname);
	Cmd_AddMacro("sb_player", SB_Macro_Player);
	Cmd_AddMacro("sb_ping", SB_Macro_Ping);
	Cmd_AddMacro("sb_map", SB_Macro_Map);
	Cmd_AddMacro("sb_ip", SB_Macro_Ip);
}
