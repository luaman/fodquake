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

static char *not_set_dummy = "not set";

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

static struct linked_list *server;

static struct ServerScanner *serverscanner;

cvar_t sb_masterserver = {"sb_masterserver", "asgaard.morphos-team.net:27000"};
cvar_t	sb_player_drawing = {"sb_player_drawing", "0"};

static int sb_open = 0;
static struct QWServer **sb_qw_server;
static unsigned int sb_qw_server_count = 0;

static int sb_help_prev = -1;
static int sb_active_help_window = 0;
#define SB_HELP_WINDOWS 4

static int sb_check_serverscanner = 0;

// general display
static int sb_active_tab = 0;
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
static char sb_filter_insert_keyword[512];
static int sb_filter_insert_keyword_position = 0;
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

static char *tab_names[12] = { "all", "duel", "2on2", "4on4", "ffa", "empty", "empty", "empty", "empty", "empty", "empty", "empty"};

struct tab
{
	int max_filter_keyword_length;
	int server_count;
	int player_count;
	int max_hostname_length;
	int max_map_length;
	int sb_position;
	int sort;
	int sort_dir;
	struct server **servers;
	int *server_index;
	char *player_filter;
	struct linked_list *filters;
};

static struct tab tabs[SB_MAX_TABS];

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
}

struct filter
{
	struct linked_list_node node;
	char *keyword;
	int type;
	/* 0 - ==
	 * 1 - <=
	 * 2 - >=
	 * 3 - isin
	 * 4 - !isin
	 */
	char *value;
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

static void SB_Set_Statusbar (const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(sb_status_bar, sizeof(sb_status_bar), format, args);
	va_end(args);
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

static int Check_Server_Against_Filter(struct tab *tab, struct QWServer *server)
{
	struct filter *fe;

	fe = (struct filter *) List_Get_Node(tab->filters, 0);
	while (fe)
	{
		if (strcmp(fe->keyword, "players") == 0)
		{
			if (fe->type == 0)
			{
				if (atoi(fe->value) != server->numplayers)
					return 0;
			}
			else if (fe->type == 1)
			{
				if (server->numplayers < atoi(fe->value))
					return 0;
			}
			else if (fe->type == 2)
			{
				if (server->numplayers > atoi(fe->value))
	 				return 0;
			}
		}
		fe = (struct filter *)fe->node.next;
	}
	return 1;
}

static int check_player(struct tab *tab, struct QWServer *server)
{
	int i;
	char *player;

	player = tab->player_filter;

	if (server->numplayers == 0 && server->numspectators == 0)
		return 0;

	for (i=0; i<server->numplayers; i++)
	{
		if (server->players[i].name)
		{
			if (strstr(server->players[i].name, player))
			{
				return 1;
			}
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
	x = sb_qw_server[*(int *)a];
	y = sb_qw_server[*(int *)b];
	return (y->numplayers - x->numplayers);
}

static int ping_compare(const void *a, const void *b)
{
	struct QWServer *x, *y;

	x = sb_qw_server[*(int *)a];
	y = sb_qw_server[*(int *)b];

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
	x = sb_qw_server[*(int *)a];
	y = sb_qw_server[*(int *)b];

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
	x = sb_qw_server[*(int *)a];
	y = sb_qw_server[*(int *)b];

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

static int stubby (struct tab *tab, struct QWServer *server)
{
	return 1;
}

static void update_tab(struct tab *tab)
{
	int count, i, x, length;
	int (*cf)(struct tab *tab, struct QWServer *server);

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
}

static void SB_Update_Tabs(void)
{
	int i;
	struct tab *tab;

	for (i=0;i<SB_MAX_TABS;i++)
	{
		tab = &tabs[i];
		update_tab(tab);
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
	if (serverscanner)
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
	struct QWServer *server;
	struct tab *tab;

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
			tab = &tabs[sb_active_tab];
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
			if (sb_selected_filter >= List_Node_Count(tabs[sb_active_tab].filters))
				sb_selected_filter = 0;
			return;
		}

		if (key == K_UPARROW)
		{
			sb_selected_filter--;		
			if (sb_selected_filter < 0)
				sb_selected_filter = List_Node_Count(tabs[sb_active_tab].filters);
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
		tab = &tabs[sb_active_tab];

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
			tab->sb_position--;
			if (tab->sb_position < 0)
			{
				tab->sb_position = tab->server_count - 1;
				if (tab->sb_position < 0)
					tab->sb_position = 0;
			}
			return;
		}

		if (key == K_DOWNARROW)
		{
			tab->sb_position++;
			if (tab->sb_position >= tab->server_count)
				tab->sb_position = 0;
			return;
		}

		if (key == K_PGDN)
		{
			tab->sb_position += 10;
			if (tab->sb_position >= tab->server_count)
				tab->sb_position = tab->server_count - 1;
			return;
		}

		if (key == K_PGUP)
		{
			tab->sb_position -= 10;
			if (tab->sb_position <= 0)
				tab->sb_position = 0;
			return;
		}

		if (key == K_ENTER)
		{
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
			Cbuf_AddText("spectator 0\n");

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

		if (key == 'f' && keydown[K_CTRL])
		{
			sb_player_filter = 1;
			return;
		}
	}

	if (key == K_ESCAPE)
	{
		SB_Close();
		return;
	}

	if (key == K_LEFTARROW)
	{
		sb_active_tab--;
		if (sb_active_tab < 0)
			sb_active_tab = SB_MAX_TABS - 1;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		sb_active_tab++;
		if (sb_active_tab >= SB_MAX_TABS)
			sb_active_tab = 0;
		return;
	}

	switch (key)
	{
		case '1':
			sb_active_tab = 0;
			break;
		case '2':
			sb_active_tab = 1;
			break;
		case '3':
			sb_active_tab = 2;
			break;
		case '4':
			sb_active_tab = 3;
			break;
		case '5':
			sb_active_tab = 4;
			break;
		case '6':
			sb_active_tab = 5;
			break;
		case '7':
			sb_active_tab = 6;
			break;
		case '8':
			sb_active_tab = 7;
			break;
		case '9':
			sb_active_tab = 8;
			break;
		case '0':
			sb_active_tab = 9;
			break;
	}
}

void SB_Activate_f(void)
{
	extern keydest_t key_dest;
	
	old_keydest = key_dest;
	key_dest = key_serverbrowser;
	sb_open = 1;

	if (serverscanner)
	{
		ServerScanner_FreeServers(serverscanner, sb_qw_server);
		ServerScanner_Delete(serverscanner);
		serverscanner = NULL;
		sb_qw_server = NULL;
		sb_qw_server_count = 0;
	}

	if (serverscanner == NULL)
	{
		serverscanner = ServerScanner_Create(sb_masterserver.string);
		if (serverscanner == NULL)
			SB_Set_Statusbar("error creating server scanner!");
	}
	SB_Update_Tabs();
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

static void SB_Add_Filter_To_Tab(int tab, char *keyword, int type, char *value)
{
	struct filter *f;
	int i;

	if (tab > 11 || tab < 0)
		return;
	
	if (strlen(keyword) == 0)
		return;

	if (strlen(value) == 0)
		return;

	f = calloc(1, sizeof(struct filter));
	if (!f)
		return;

	f->keyword = strdup(keyword);

	if (!f->keyword)
	{
		free(f);
		return;
	}	
	f->type = type;
	f->value = strdup(value);
	if (!f->value)
	{
		free(f->keyword);
		free(f);
		return;
	}

	List_Add_Node(tabs[tab].filters, f);
	i = strlen(keyword);
	if (tabs[tab].max_filter_keyword_length < i)
		tabs[tab].max_filter_keyword_length = i;
}

static void SB_Filter_Insert_Handler(int key)
{
	if (key == K_ENTER)
	{
		SB_Add_Filter_To_Tab(sb_active_tab, sb_filter_insert_keyword, sb_filter_insert_selected_type, sb_filter_insert_value);
		sb_filter_insert = 0;
		sb_filter_insert_keyword_position = sb_filter_insert_value_position = 0;
		update_tab(&tabs[sb_active_tab]);
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
		if (key == K_UPARROW)
			sb_filter_insert_selected_type++;
		if (sb_filter_insert_selected_type< 0)
			sb_filter_insert_selected_type = 4;
		if (sb_filter_insert_selected_type> 4)
			sb_filter_insert_selected_type = 0;
		return;
	}

	if (sb_filter_insert_selected_box == 2)
	{
		handle_textbox(key, sb_filter_insert_value, &sb_filter_insert_value_position, sizeof(sb_filter_insert_value));
		return;
	}
	
	if (sb_filter_insert_selected_box == 0)
	{
		handle_textbox(key, sb_filter_insert_keyword, &sb_filter_insert_keyword_position, sizeof(sb_filter_insert_keyword));
		return;
	}
}

void SB_Filter_Delete_Filter(void)
{
	List_Remove_Node(tabs[sb_active_tab].filters, sb_selected_filter, 1);
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
	int width, tabs_width;
	int x,i, diff;
	static char tabstring[512];
	static double last_time = 0;
	static int last_tab = -1;
	static int current_position = 0;
	static int desired_position = 0;

	width = vid.conwidth/8;

	// will be returned by a function
	if (last_tab != sb_active_tab)
	{
		tabstring[0] = '\0';
		last_time = cls.realtime;
		last_tab = sb_active_tab;

		for (i = 0; i < SB_MAX_TABS; i++)
		{
			if (i == sb_active_tab)
				strlcat(tabstring, va("<%s>", tab_names[i]), sizeof(tabstring));
			else
				strlcat(tabstring, va(" %s ", tab_names[i]), sizeof(tabstring));
		}
		tabs_width = strlen(tabstring);
	}
	else
	{
		if (cls.realtime - last_time < 1)
		{
			x = (desired_position-current_position) *((cls.realtime-last_time)/1) + current_position;
		}
		else
		{
			x = current_position = desired_position;
		}
		Draw_String(x, 8, tabstring);
		return;
	}

	diff = 0;
	if (width < tabs_width)
		for (i=0;i<sb_active_tab && diff<(tabs_width - width) ;i++)
			diff += strlen(tab_names[i]);
	
	desired_position = -diff * 8;

	Draw_String(current_position, 8, tabstring);
}

static void SB_Draw_Background(void)
{
	Draw_Fill(0, 0, vid.conwidth, vid.conheight, 1);
}

static void SB_Draw_Filter_Insert(void)
{
	Draw_String(8,24, sb_filter_insert_keyword);
	if (sb_filter_insert_selected_box == 0)
		Draw_String(8 + 8*sb_filter_insert_keyword_position, 24, "_");
	Draw_String(8, 32, Filter_Type_String(sb_filter_insert_selected_type));
	Draw_String(8, 40, sb_filter_insert_value);
	if (sb_filter_insert_selected_box == 2)
		Draw_String(8 + 8*sb_filter_insert_value_position, 40, "_");
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
	filter = List_Get_Node(tabs[sb_active_tab].filters, 0);
	i = 0;
	while(filter)
	{
		if (i == sb_selected_filter)
		{
			Draw_String(0, 24+i*8,">");
		}
		Draw_String(8, 24 + i++ * 8, va("%*s %5s %s", tabs[sb_active_tab].max_filter_keyword_length, filter->keyword, Filter_Type_String(filter->type), filter->value)); 
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
		if ((i = strcmp(x->team ,y->team)) == 0)
			return (y->frags - x->frags);
		else 
			return i;
	
	return (y->frags - x->frags);
}

static int sort_players(const void *a, const void *b)
{
	struct QWPlayer *x, *y;
	int i;

	x = *(struct QWPlayer **)a;
	y = *(struct QWPlayer **)b;
	
	return (y->frags - x->frags);
}

static void SB_Draw_Server(void)
{
	int i, x, y, z, team_width;
	int line_space, player_space;
	int offset;
	struct tab *tab;
	char *hostname, *map;
	char string[512];
	int position;
	struct QWPlayer **sorted_players;
	struct QWServer *server;
	struct QWPlayer *player;
	const struct QWSpectator *spectator;

	if (sb_server_insert)
	{
		SB_Draw_Server_Insert();
		return;
	}

	tab = &tabs[sb_active_tab];

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
		Draw_String(8, 8 + 8 * i++, " F1->F12, arrow left/right -  to switch tabs");
		Draw_String(8, 8 + 8 * i++, " pgup/pgdown, arrow up/down - to scroll");
		Draw_String(8, 8 + 8 * i++, " esc - will quit submenus, browser");
		
	}
	else if (sb_active_help_window == 1)
	{
		Draw_String(8, 8 + 8 * i++, "server screen:");
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

void SB_Init(void)
{
	int i;
	Cmd_AddCommand("sb_activate", &SB_Activate_f);
	Cvar_Register(&sb_masterserver);
	Cvar_Register(&sb_player_drawing);

	for (i=0;i<SB_MAX_TABS;i++)
	{
		tabs[i].filters = List_Add(0, NULL, NULL);
	}

	server = List_Add(0, NULL, NULL);

	SB_Set_Statusbar("just started!. press \"ctrl + h\" for help\n");
}

