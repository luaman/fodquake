#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "context_sensitive_tab.h"
#include "tokenize_string.h"
#include "text_input.h"

enum CSTC_Pictures
{
	cstcp_magnifying,
	cstcp_arrow_up,
	cstcp_arrow_down,
	cstcp_arrow_left,
	cstcp_arrow_right
};

struct cst_commands
{
	struct cst_commands *next;
	char *name;
	struct tokenized_string *commands;
	int (*conditions)(void);
	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
	int flags;
	char *tooltip;
};

struct cst_commands Command_Completion;
struct Picture *cstc_pictures;

#define MAXCMDLINE 256
extern int key_linepos;
extern int edit_line;
extern char key_lines[32][MAXCMDLINE];

int context_sensitive_tab_completion_active = 0;

cvar_t	context_sensitive_tab_completion = {"context_sensitive_tab_completion", "1"};
cvar_t	context_sensitive_tab_completion_command_only_on_ctrl_tab = {"context_sensitive_tab_completion_command_only_on_ctrl_tab", "1"};
cvar_t	context_sensitive_tab_completion_color_coded_types = {"context_sensitive_tab_completion_color_coded_types", "1"};
cvar_t	context_sensitive_tab_completion_close_on_tab = {"context_sensitive_tab_completion_close_on_tab", "1"};
cvar_t	context_sensitive_tab_completion_sorting_method = {"context_sensitive_tab_completion_sorting_method", "2"};
cvar_t	context_sensitive_tab_completion_show_results = {"context_sensitive_tab_completion_show_results", "1"};
cvar_t	context_sensitive_tab_completion_ignore_alt_tab = {"context_sensitive_tab_completion_ignore_alt_tab", "1"};
cvar_t	context_sensitive_tab_completion_background_color = {"context_sensitive_tab_completion_background_color", "4"};
cvar_t	context_sensitive_tab_completion_tooltip_color = {"context_sensitive_tab_completion_tooltip_color", "14"};
cvar_t	context_sensitive_tab_completion_inputbox_color = {"context_sensitive_tab_completion_inputbox_color", "4"};
cvar_t	context_sensitive_tab_completion_selected_color = {"context_sensitive_tab_completion_selected_color", "40"};
cvar_t	context_sensitive_tab_completion_highlight_color = {"context_sensitive_tab_completion_highlight_color", "186"};
cvar_t	context_sensitive_tab_completion_insert_slash = {"context_sensitive_tab_completion_insert_slash", "1"};
cvar_t	context_sensitive_tab_completion_slider_no_offset = {"context_sensitive_tab_completion_slider_no_offset", "1"};
cvar_t	context_sensitive_tab_completion_slider_border_color = {"context_sensitive_tab_completion_slider_border_color", "0"};
cvar_t	context_sensitive_tab_completion_slider_background_color = {"context_sensitive_tab_completion_slider_background_color", "4"};
cvar_t	context_sensitive_tab_completion_slider_color = {"context_sensitive_tab_completion_slider_color", "10"};
cvar_t	context_sensitive_tab_completion_slider_variables = {"context_sensitive_tab_completion_slider_variables", "gl_gamma gl_contrast volume"};
cvar_t	context_sensitive_tab_completion_execute_on_enter = {"context_sensitive_tab_completion_execute_on_enter", "quit sb_activate"};

char *context_sensitive_tab_completion_color_variables = "context_sensitive_tab_completion_inputbox_color context_sensitive_tab_completion_selected_color context_sensitive_tab_completion_background_color sb_color_bg sb_color_bg_empty sb_color_bg_free sb_color_bg_specable sb_color_bg_full sb_highlight_sort_column_color topcolor bottomcolor r_skycolor context_sensitive_tab_completion_slider_border_color context_sensitive_tab_completion_slider_background_color context_sensitive_tab_completion_slider_color context_sensitive_tab_completion_tooltip_color context_sensitive_tab_completion_highlight_color";
char *context_sensitive_tab_completion_player_color_variables = "teamcolor enemycolor color";

char *cstc_slider_tooltip = "arrow up/down will add/remove 0.1, arrow left/right will add/remove 0.01";
char *cstc_player_color_tooltip = "arrow keys to navigate, space to select the color, enter to finalize";
char *cstc_color_tooltip = "arror keys to navigate, enter to select";

static void cleanup_cst(struct cst_info *info)
{
	if (info == NULL)
		return;

	if (info->tokenized_input)
		Tokenize_String_Delete(info->tokenized_input);

	if (info->get_data)
		info->get_data(info, 1);

	if (info->checked)
		free(info->checked);

	if (info->new_input)
		Text_Input_Delete(info->new_input);
}

static struct cst_info cst_info_static;
static struct cst_info *cst_info = &cst_info_static;

static struct cst_commands *commands;
static struct cst_commands CC_Slider;
static struct cst_commands CC_Player_Color;
static struct cst_commands CC_Color;

static void CSTC_Cleanup(struct cst_info *self)
{
	context_sensitive_tab_completion_active = 0;
	cleanup_cst(self);
	memset(self, 0, sizeof(struct cst_info));
}

static void CSTC_Draw_Picture(int x, int y, int width, int height, enum CSTC_Pictures pic)
{
	int index_x, index_y, iw, ih;
	float sx, sy;

	if (cstc_pictures == NULL)
		return;

	for (index_x = pic, index_y = 0; index_x > 16; index_x -= 16, index_y++);

	sx = (1.0f/16.0f) * index_x;
	sy = (1.0f/16.0f) * index_y;
	Draw_DrawSubPicture(cstc_pictures, (1.0f/16.0f) * index_x, (1.0f/16.0f) * index_y, 1.0f/16.0f, 1.0f/16.0f, x, y, width ? width : 16, height ? height : 16);
}

qboolean CSTC_Execute_On_Enter(char *cmd)
{
	struct tokenized_string *ts;
	qboolean rval = false;
	int i;

	ts = Tokenize_String(context_sensitive_tab_completion_execute_on_enter.string);
	if (ts)
	{
		for (i=0; i<ts->count; i++)
		{
			if (strcmp(cmd, ts->tokens[i]) == 0)
			{
				rval = true;
				break;
			}
		}
		Tokenize_String_Delete(ts);
	}
	return rval;
}

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), int flags, char *tooltip)
{
	struct cst_commands *command, *cc;
	char *in;

	if (name == NULL || result == NULL)
		return;

	in = strdup(name);
	if (in == NULL)
		return;

	command = (struct cst_commands *) calloc(1, sizeof(struct cst_commands));

	if (command == NULL)
	{
		free(in);
		return;
	}

	if (commands == NULL)
		commands = command;
	else
	{
		cc = commands;
		while (cc->next)
			cc = cc->next;
		cc->next = command;
	}

	command->name = in;
	command->conditions = conditions;
	command->result = result;
	command->get_data = get_data;
	command->flags = flags;
	if (flags & CSTC_MULTI_COMMAND)
		command->commands = Tokenize_String(command->name);
	command->tooltip = tooltip;
}

static void Tokenize_Input(struct cst_info *self)
{
	if (self == NULL)
		return;

	if (self->tokenized_input)
	{
		Tokenize_String_Delete(self->tokenized_input);
		self->tokenized_input= NULL;
	}

	self->tokenized_input = Tokenize_String(self->input);
}

static void insert_result(struct cst_info *self, char *ptr)
{
	char *result;
	char new_keyline[MAXCMDLINE];
	int i;

	if (ptr)
		result = ptr;
	else
		if (cst_info->result(cst_info, NULL, cst_info->selection, 0, &result))
			return;

	snprintf(new_keyline, sizeof(new_keyline),
			"%*.*s%s%s%s%s ",
			self->command_start, self->command_start, key_lines[edit_line],
			(context_sensitive_tab_completion_insert_slash.value == 1 && self->argument_start == 1 && key_lines[edit_line][1] != '/') ? "/" : "",
			self->flags & CSTC_COMMAND ? "" : self->real_name,
			self->flags & CSTC_COMMAND ? "" : " ",
			result
			);

	memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);
	key_linepos = self->argument_start + strlen(result);

	key_linepos++;
	if (self->flags & CSTC_COMMAND)
		key_linepos++;

	i = strlen(key_lines[edit_line]);
	if (key_linepos > i)
		key_linepos = i;

	if (key_linepos >= MAXCMDLINE)
		key_linepos = MAXCMDLINE - 1;
}

void CSTC_Insert_And_Close(void)
{
	insert_result(cst_info, NULL);
	context_sensitive_tab_completion_active = 0;
}

static void cstc_insert_only_find(struct cst_info *self)
{
	insert_result(self, NULL);
	if (CSTC_Execute_On_Enter(self->real_name))
		Cbuf_AddText("\n");
	CSTC_Cleanup(self);
}

static int valid_color(int i)
{
	if (i < 8)
		return i * 16 + 15;
	else
		return i *16;
}

void Key_Console (int key);
void Context_Sensitive_Tab_Completion_Key(int key)
{
	int i;
	qboolean execute = false;

	if (context_sensitive_tab_completion_active == 0)
		return;

	//toggle tooltip
	if (keydown[K_CTRL] && key == 'h')
	{
		cst_info->tooltip_show = !cst_info->tooltip_show;
		return;
	}

	// hide tooltip if any other key is pressed
	if (cst_info->tooltip_show)
		cst_info->tooltip_show = false;

	if (cst_info->flags & CSTC_COLOR_SELECTOR)
		i = 256;
	else
		i = cst_info->results;

	if (key == K_ESCAPE)
	{
		if (cst_info->flags & CSTC_SLIDER)
			if (cst_info->variable)
				Cvar_Set(cst_info->variable, va("%f", cst_info->slider_original_value));
		CSTC_Cleanup(cst_info);
		return;
	}

	// wanted to use F1->F12 here
	if (keydown[K_CTRL] && key >= '0' && key <= '9')
	{
		i = key - '0';
		i--;
		if (i<0)
			i+=10;
		cst_info->toggleables[i] = !cst_info->toggleables[i];
		cst_info->changed = true;
		if (!(cst_info->flags & CSTC_NO_INPUT) && !(cst_info->flags & CSTC_COLOR_SELECTOR))
		{
			cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
			cst_info->selection = 0;
		}
		return;
	}


	if (key == K_ENTER)
	{
		insert_result(cst_info, NULL);
		if ((CSTC_Execute_On_Enter(cst_info->real_name) || cst_info->flags & CSTC_EXECUTE) && !keydown[K_CTRL])
			execute = true;
		CSTC_Cleanup(cst_info);
		if (execute)
			Key_Console(K_ENTER); //there might be a better way to do this :P
		return;
	}

	if (cst_info->flags & CSTC_SLIDER)
	{
		if (key == K_LEFTARROW)
			cst_info->slider_value -= 0.01;

		if (key == K_RIGHTARROW)
			cst_info->slider_value += 0.01;

		if (key == K_DOWNARROW)
			cst_info->slider_value -= 0.1;

		if (key == K_UPARROW)
			cst_info->slider_value += 0.1;

		cst_info->slider_value = bound(0, cst_info->slider_value, 1);
		if (cst_info->variable)
			Cvar_Set(cst_info->variable, va("%f", cst_info->slider_value));
		return;
	}

	if (cst_info->flags & CSTC_COLOR_SELECTOR)
	{
		if (key == K_LEFTARROW)
		{
			cst_info->selection--;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
			return;
		}

		if (key == K_RIGHTARROW)
		{
			cst_info->selection++;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
			return;
		}

		if (key == K_UPARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection -= 16;
			else
				cst_info->selection += 16;

			if (cst_info->selection < 0)
				cst_info->selection = i + cst_info->selection;
			if (cst_info->selection >= i)
				cst_info->selection = cst_info->selection - i;
			return;
		}

		if (key == K_DOWNARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection += 16;
			else
				cst_info->selection -= 16;

			if (cst_info->selection < 0)
				cst_info->selection = i + cst_info->selection;
			if (cst_info->selection >= i)
				cst_info->selection = cst_info->selection - i;

			return;
		}
	}
	else if (cst_info->flags & CSTC_PLAYER_COLOR_SELECTOR)
	{
		if (key == K_SPACE)
		{
			cst_info->color[cst_info->count] = cst_info->selection;
			return;
		}

		if (key == K_UPARROW || key == K_DOWNARROW)
		{
			cst_info->count += 1 * (cst_info->direction == 1 ? 1 : -1) * (key == K_UPARROW ? -1 : 1);
			cst_info->count = bound(0, cst_info->count, 1);
			return;
		}

		if (key == K_LEFTARROW || key == K_RIGHTARROW)
		{
			cst_info->selection += 1 * key == K_LEFTARROW ? -1 : 1;
			cst_info->selection = bound(0, cst_info->selection, 13);
			return;
		}
	}
	else
	{
		if (key == K_UPARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection--;
			else
				cst_info->selection++;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
			return;
		}

		if (key == K_DOWNARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection++;
			else
				cst_info->selection--;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
			return;
		}
	}

	if (key == K_TAB)
	{
		if (context_sensitive_tab_completion_close_on_tab.value == 1)
		{
			CSTC_Cleanup(cst_info);
		}
		else
		{
			if (cst_info->direction == 1)
				cst_info->selection++;
			else
				cst_info->selection--;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
		}
		return;
	}


	if (!(cst_info->flags & CSTC_NO_INPUT) && !(cst_info->flags & CSTC_COLOR_SELECTOR))
	{
		Text_Input_Handle_Key(cst_info->new_input, key);
		Tokenize_Input(cst_info);
		cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
	}
	return;
}

static void CSTC_Draw(struct cst_info *self, int y_offset)
{
	int i, j, result_offset, offset, rows, sup, sdown, x, y, pos_x, pos_y, sp_x, sp_y;
	char *ptr, *ptr_result, *s;
	char buf[128];

	if (self->direction == -1)
		offset = y_offset - 32;
	else
		offset = y_offset - 14;

	if (!(self->flags & CSTC_NO_INPUT) && !(self->flags & CSTC_COLOR_SELECTOR))
	{
		Draw_Fill(0, offset , vid.conwidth, 10, context_sensitive_tab_completion_inputbox_color.value);
		Draw_String(8, offset, self->input);
		Draw_String(8 + self->new_input->position * 8 , offset + 2, "_");
		offset += 8 * self->direction;
	}

	if (self->direction == -1)
		rows = offset / 8;
	else
		rows = (vid.conheight - offset) / 8;

	if (rows % 2 != 0)
		rows--;


	result_offset = sup = sdown = 0;

	if (rows < self->results)
	{
		sup = 1;

		if (self->selection > rows/2)
		{
			result_offset = self->selection - rows/2;
			sdown = 1;
		}

		if ((self->results - self->selection) < rows/2)
		{
			result_offset = self->results - rows;
			sup = 0;
		}
	}

	if (self->flags & CSTC_COLOR_SELECTOR)
	{
		sp_x = sp_y = 0;
		for (x=0; x<16; x++)
		{
			for (y=0; y<16; y++)
			{
				pos_x = self->argument_start * 8 + 16;
				if (pos_x > vid.displaywidth/2)
					pos_x -= 16 *8;
				pos_x += x *8;
				pos_y = offset + y * 8 * self->direction;
				Draw_Fill(pos_x, pos_y, 8, 8, x + y * 16);
				if (self->selection == x + y * 16)
				{
					sp_x = pos_x;
					sp_y = pos_y;
				}
			}
		}
		Draw_String(sp_x, sp_y, "x");
	}
	else if (self->flags & CSTC_PLAYER_COLOR_SELECTOR)
	{
		pos_x = self->argument_start * 8 + 16;
		if (pos_x > vid.displaywidth/2)
			pos_x -= 16 *8 + 4;
		pos_y = offset;
		Draw_Fill(pos_x - 2, pos_y + 8 * self->direction - 2, 6 * 8 + 4 + 4, 16 + 4, 0);
		Draw_String(pos_x, pos_y, self->direction > 0 ? "top" : "bottom");
		Draw_String(pos_x, pos_y + 8 * self->direction, self->direction > 0 ? "bottom" : "top");
		pos_x += 6 * 8 + 4;
		for (y=0; y<2; y++)
		{
			for (x=0; x<13; x++)
			{
				if (x<8)
					i = x * 16 + 15;
				else
					i = x * 16;
				Draw_Fill(pos_x + x *8, pos_y + y * 8 * self->direction, 8, 8, i);
				if (y == 0 && x == self->color[0])
					Draw_String(pos_x + x *8, pos_y + y * 8 * self->direction, "o");
				if (y == 1 && x == self->color[1])
					Draw_String(pos_x + x *8, pos_y + y * 8 * self->direction, "o");
			}
		}
		Draw_String(pos_x + self->selection * 8, pos_y + self->count * self->direction * 8, "x");
	}
	else if (self->flags & CSTC_SLIDER)
	{
		pos_x = self->argument_start * 8 + 16;
		if (context_sensitive_tab_completion_slider_no_offset.value)
			pos_y = offset - self->direction * 8;
		else 
			pos_y = offset;
		Draw_Fill(pos_x, pos_y, 8*8 + 2, 8, context_sensitive_tab_completion_slider_border_color.value);
		Draw_Fill(pos_x+1, pos_y+1, 8*8, 8, context_sensitive_tab_completion_slider_background_color.value);
		Draw_Fill(pos_x+1, pos_y+1, 8*8 * self->slider_value,8, context_sensitive_tab_completion_slider_color.value);
		Draw_String(pos_x , pos_y, va("%f", self->slider_value));
	}
	else
	{
		for (i=0, ptr = NULL; i<rows; i++)
		{
			if (self->result(self, NULL, i + result_offset, cstc_rt_draw, &ptr))
				break;
			if (self->flags & CSTC_COMMAND || self->flags & CSTC_HIGLIGHT_INPUT)
				self->result(self, NULL, i + result_offset, cstc_rt_highlight, &ptr_result);

			if (i + result_offset == self->selection)
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_selected_color.value);
			else
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_background_color.value);

			if ((self->flags & CSTC_COMMAND || self->flags & CSTC_HIGLIGHT_INPUT) && ptr_result)
			{
				for (j=0; j<self->tokenized_input->count; j++)
				{
					s = Util_strcasestr(ptr_result, self->tokenized_input->tokens[j]);
					if (s)
					{
						x = s - ptr_result;
						Draw_Fill(32 + x * 8, offset + i * 8 * self->direction, strlen(self->tokenized_input->tokens[j]) * 8, 8, context_sensitive_tab_completion_highlight_color.value);
					}
				}
			}

			if (ptr)
				Draw_ColoredString(32, offset + i * 8 * self->direction, ptr, 0);
		}

		if (sup)
		{
			/*Draw_String(8, offset + (rows - 1) * 8 * self->direction, "^");*/
			CSTC_Draw_Picture(8, offset + (rows - 1) * 8 * self->direction, 0, 0, cstcp_arrow_up);
		}

		if (sdown)
		{
			/*Draw_String(8, offset + 8 * self->direction, "v");*/
			CSTC_Draw_Picture(8, offset + 8 * self->direction, 0, 0, cstcp_arrow_down);
		}

		if (rows < self->results && context_sensitive_tab_completion_show_results.value == 1)
		{
			sprintf(buf, "showing %i of %i results", rows, self->results);
			Draw_String(vid.conwidth - strlen(buf) * 8, offset, buf);
		}
	}

	if (self->tooltip_show && self->tooltip)
	{
		Draw_Fill(0, offset , vid.conwidth, 8, context_sensitive_tab_completion_tooltip_color.value);
		Draw_String(0, offset , va("help: %s", self->tooltip));
	}



}

void Context_Sensitive_Tab_Completion_Draw(void)
{
	extern float scr_conlines;

	if (context_sensitive_tab_completion_active == 0)
		return;

	if (cst_info == NULL)
		return;

	if (scr_conlines > vid.conheight/2)
		cst_info->direction = -1;
	else
		cst_info->direction = 1;

	if (cst_info->results == 1)
	{
		cstc_insert_only_find(cst_info);
		return;
	}

	CSTC_Draw(cst_info, scr_conlines);
}

static void getarg(const char *s, char **start, char **end, char **next, qboolean cmd)
{
	char endchar;

	while(*s == ' ')
		s++;

	if (*s == '"')
	{
		endchar = '"';
		s++;
	}
	else
		if (cmd)
			endchar = ' ';
		else
			endchar = ';';

	*start = (char *)s;

	while(*s && *s != endchar && (endchar != ' ' || *s != ';'))
		s++;

	*end = (char *)s;

	if (*s == '"')
		s++;

	*next = (char *)s;
}

void read_info_new (char *string, int position, char **cmd_begin, int *cmd_len, char **arg_begin, int *arg_len, int *cursor_on_command, int *is_invalid)
{
	char *cmd_start, *cmd_stop, *arg_start, *arg_stop;
	char **start, **stop;
	char *s;
	char *next;
	int docontinue;
	int dobreak;
	int isinvalid;
	qboolean cmd = true;

	docontinue = 0;
	dobreak = 0;

	s = string;

	cmd_start = string;
	cmd_stop = string;
	arg_start = string;
	arg_stop = string;

	isinvalid = 0;

	if (cursor_on_command)
		*cursor_on_command = 0;


	while(*s)
	{
		if (*s == '/')
		{
			if (s != string)
				isinvalid = 1;

			s++;
		}

		if (*s == ' ')
			break;

		if (position < s - string)
			break;

		start = &cmd_start;
		stop = &cmd_stop;

		while(*s)
		{
			getarg(s, start, stop, &next, cmd);

#if 0
			printf("'%s' '%s'\n", cmd_start, cmd_stop);
#endif

			if (dobreak)
				break;

			s = next;
			while(*s == ' ')
				s++;

			if (position >= (*start - string) && position <= (next - string))
			{
#if 0
				printf("Cursor is in command\n");
#endif
				if (cursor_on_command)
					*cursor_on_command = 1;

				dobreak = 1;

#if 0
				if (*s == 0 || *s == ';')
#endif
					break;
			}

			if (*s == ';')
			{
				isinvalid = 0;

				s++;
				while(*s == ' ')
					s++;

				cmd_start = string;
				cmd_stop = string;
				arg_start = string;
				arg_stop = string;

				docontinue = 1;
				break;
			}

			start = &arg_start;
			stop = &arg_stop;
			cmd = false;
		}

		if (docontinue)
		{
			docontinue = 0;
			continue;
		}

		if (dobreak)
			break;
	}

	if (isinvalid)
	{
		cmd_start = string;
		cmd_stop = string;
		arg_start = string;
		arg_stop = string;

		if (cmd_begin)
			*cmd_begin = NULL;

		if (cmd_len)
			cmd_len = 0;

		if (arg_begin)
			*arg_begin = NULL;

		if (arg_len)
			*arg_len = 0;

		if (is_invalid)
			*is_invalid = 1;

	}
	else
	{
		if (cmd_begin)
			*cmd_begin = cmd_start;

		if (cmd_len)
			*cmd_len = cmd_stop - cmd_start;

		if (arg_begin)
			*arg_begin = arg_start;

		if (arg_len)
			*arg_len = arg_stop - arg_start;

		if (is_invalid)
			*is_invalid = 0;
	}

	/*
	printf(" %s\n", string);
	printf("%*s%s\n", position + 1, " ", "_");
	printf("%*s%s\n", cmd_start - string + 1, " ", "^");
	printf("%*s%s\n", cmd_stop - string + 1, " ", "^");
	printf("%*s%s\n", arg_start - string + 1, " ", "^");
	printf("%*s%s\n", arg_stop - string + 1, " ", "^");
	*/
}

static void setup_completion(struct cst_commands *cc, struct cst_info *c, int arg_start, int arg_len, int cmd_start, int cmd_len)
{
	if (c == NULL || cc == NULL)
		return;

	memset(c, 0, sizeof(struct cst_info));

	c->name = cc->name;
	c->commands = cc->commands;
	c->result = cc->result;
	c->get_data = cc->get_data;
	snprintf(c->input, sizeof(c->input), "%.*s", arg_len, key_lines[edit_line] + arg_start);

	c->new_input = Text_Input_Create(c->input, sizeof(c->input), arg_len, 0);

	Tokenize_Input(c);
	c->argument_start = arg_start;
	c->argument_length = arg_len;
	c->command_start = cmd_start;
	c->command_length = cmd_len;
	if (c->get_data)
		c->get_data(c, 0);
	c->result(c, &c->results, 0, 0, NULL);
	c->flags = cc->flags;
	c->tooltip = cc->tooltip;
}

static void setup_slider(struct cst_info *c)
{
	if (c->variable)
	{
		c->slider_value = bound(0, c->variable->value, 1);
		c->slider_original_value = c->variable->value;
	}
	else
	{
		c->slider_original_value = c->slider_value = 0;
	}
}

static int setup_current_command(void)
{
	int cmd_len, arg_len, cursor_on_command, isinvalid, i, dobreak;
	char *cmd_start, *arg_start, *name;
	int cmd_istart, arg_istart;
	struct cst_commands *c;
	cvar_t *var;
	char new_keyline[MAXCMDLINE], *cs;
	struct tokenized_string *ts, *var_ts;
	qboolean cvar_setup;

	read_info_new(key_lines[edit_line] + 1, key_linepos, &cmd_start, &cmd_len, &arg_start, &arg_len, &cursor_on_command, &isinvalid);

	if (isinvalid)
		return 0;

	cs = key_lines[edit_line];

	cmd_istart = cmd_start - key_lines[edit_line];
	if (arg_start)
		arg_istart = arg_start - key_lines[edit_line];
	else
		arg_istart = cmd_istart + cmd_len;

	if (arg_istart < cmd_istart)
		arg_istart = cmd_istart + cmd_len;

	if (cursor_on_command || key_lines[edit_line] + key_linepos == cmd_start + cmd_len)
	{
		if ((context_sensitive_tab_completion_command_only_on_ctrl_tab.value == 1 && keydown[K_CTRL]) || context_sensitive_tab_completion_command_only_on_ctrl_tab.value == 0)
		{
			setup_completion(&Command_Completion, cst_info, cmd_istart , cmd_len, cmd_istart, cmd_len);
			return 1;
		}
	}

	if (cmd_start && arg_start)
	{
		c = commands;
		dobreak = 0;

		while (c)
		{
			if (c->flags & CSTC_MULTI_COMMAND)
			{
				for (i=0; i<c->commands->count; i++)
				{
					if (cmd_len == strlen(c->commands->tokens[i]) && strncasecmp(c->commands->tokens[i], cmd_start, cmd_len) == 0)
					{
						dobreak = 1;
						name = c->commands->tokens[i];
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
						break;
					}
				}
			}
			else
			{
				if (cmd_len == strlen(c->name) && strncasecmp(c->name, cmd_start, cmd_len) == 0)
				{
					name = c->name;
					snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					break;
				}
			}

			if (dobreak)
				break;
			c = c->next;
		}

		// check user set variables
		if (!c)
		{
			// slider
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_slider_variables.string);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Slider;
						setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);

						cst_info->variable = Cvar_FindVar(name);
						setup_slider(cst_info);
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return 1;
			}

			// player_color
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_player_color_variables);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Player_Color;
						var = Cvar_FindVar(name);
						setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);

						if (var)
						{
							var_ts = Tokenize_String(var->string);
							if (var_ts)
							{
								if (var_ts->count == 1)
								{
									cst_info->color[0] = cst_info->color[1] = atof(var_ts->tokens[0]);
								}
								else if (var_ts->count == 2)
								{
									cst_info->color[0] = atof(var_ts->tokens[0]);
									cst_info->color[1] = atof(var_ts->tokens[1]);
								}
								Tokenize_String_Delete(var_ts);
							}
						}
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return 1;
			}

			// color selector
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_color_variables);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Color;
						var = Cvar_FindVar(name);
						setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);
						if (var)
							cst_info->selection = var->value;
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return 1;
			}
		}

		if (c)
		{
			if (c->conditions)
				if (c->conditions() == 0)
					return 0;
			setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);
			cst_info->function = Cmd_FindCommand(name);
			cst_info->variable = Cvar_FindVar(name);
			snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
			if (cst_info->flags & CSTC_SLIDER)
				setup_slider(cst_info);
			return 1;
		}
		else
		{
			snprintf(new_keyline, sizeof(new_keyline), "%*.*s", cmd_len, cmd_len, cmd_start);
			var = Cvar_FindVar(new_keyline);
			if (var)
			{
				snprintf(new_keyline, sizeof(new_keyline), "%s\"%s\"", key_lines[edit_line], var->string);
				key_linepos = strlen(new_keyline);
				memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);
			}
		}
	}
	return 0;
}

int Context_Sensitive_Tab_Completion(void)
{
	if (context_sensitive_tab_completion_ignore_alt_tab.value == 1)
		if (keydown[K_ALT])
			return 0;

	if (setup_current_command())
	{
		context_sensitive_tab_completion_active = 1;
		return 1;
	}

	return 0;
}

int Cmd_CompleteCountPossible (char *partial);
int Cmd_AliasCompleteCountPossible (char *partial);
int Cvar_CompleteCountPossible (char *partial);

struct cva_s
{
	union
	{
		cmd_function_t *c;
		cmd_alias_t *a;
		cvar_t *v;
	} info;
	int type;
	unsigned int match;
};

static int name_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	char *na, *nb;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			na = x->info.c->name;
			break;
		case 1:
			na = x->info.a->name;
			break;
		case 2:
			na = x->info.v->name;
			break;
		default:
			return -1;
	}

	switch (y->type)
	{
		case 0:
			nb = y->info.c->name;
			break;
		case 1:
			nb = y->info.a->name;
			break;
		case 2:
			nb = y->info.v->name;
			break;
		default:
			return -1;
	}

	return(strcasecmp(na, nb));
}

static int new_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	char *na, *nb;
	int w1, w2;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			na = x->info.c->name;
			w1 = x->info.c->weight;
			break;
		case 1:
			na = x->info.a->name;
			w1 = x->info.a->weight;
			break;
		case 2:
			na = x->info.v->name;
			w1 = x->info.v->weight;
			break;
		default:
			return -1;
	}

	switch (y->type)
	{
		case 0:
			nb = y->info.c->name;
			w2 = y->info.c->weight;
			break;
		case 1:
			nb = y->info.a->name;
			w2 = y->info.a->weight;
			break;
		case 2:
			nb = y->info.v->name;
			w2 = y->info.v->weight;
			break;
		default:
			return -1;
	}

	return(strcasecmp(na, nb) + (x->match - y->match) +  (w2 - w1));
}


static int match_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	int w1, w2;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			w1 = x->info.c->weight;
			break;
		case 1:
			w1 = x->info.a->weight;
			break;
		case 2:
			w1 = x->info.v->weight;
			break;
		default:
			w1 = 0;
	}

	switch (y->type)
	{
		case 0:
			w2 = y->info.c->weight;
			break;
		case 1:
			w2 = y->info.a->weight;
			break;
		case 2:
			w2 = y->info.v->weight;
			break;
		default:
			w2 = 0;
	}

	return x->match - y->match + w2 *2 - w1 *2;
}

static int setup_command_completion_data(struct cst_info *self)
{
	extern cvar_t *cvar_vars;
	extern cmd_function_t *cmd_functions;
	extern cmd_alias_t *cmd_alias;
	cmd_function_t *cmd;
	cmd_alias_t *alias;
	cvar_t *var;
	int count, i, add, match;

	char *s;

	struct cva_s *cd;

	if (self->data)
	{
		free(self->data);
		self->data = NULL;
	}

	count = 0;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (Util_strcasestr(cmd->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (Util_strcasestr(alias->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	for (var=cvar_vars; var; var=var->next)
	{
		add = 1;

		for (i=0; i<self->tokenized_input->count; i++)
		{

			if (Util_strcasestr(var->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	if (count == 0)
		return -1;

	self->data = calloc(count, sizeof(struct cva_s));
	if (self->data == NULL)
		return -1;

	cd = self->data;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
	{
		add = 1;
		match = 0;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(cmd->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
			match += s - cmd->name;
		}

		if (add)
		{
			cd->info.c = cmd;
			cd->type = 0;
			cd->match = match + cmd->weight;
			cd++;
		}
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		match = 0;
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(alias->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
				match += s - alias->name;
		}

		if (add)
		{
			cd->info.a = alias;
			cd->type = 1;
			cd->match = match + alias->weight;
			cd++;
		}
	}

	for (var=cvar_vars; var; var=var->next)
	{
		match = 0;
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(var->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
			match += s - var->name;
		}
		if (add)
		{
			cd->info.v = var;
			cd->type = 2;
			cd->match = match + var->weight;
			cd++;
		}
	}

	cd = self->data;

	if (context_sensitive_tab_completion_sorting_method.value == 1)
		qsort(self->data, count, sizeof(struct cva_s), &match_compare);
	else if (context_sensitive_tab_completion_sorting_method.value == 2)
		qsort(self->data, count, sizeof(struct cva_s), &new_compare);
	else
		qsort(self->data, count, sizeof(struct cva_s), &name_compare);

	return count;
}

static int Command_Completion_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	int count;
	struct cva_s *cc;
	char *res;
	char *t, *s;

	if (self == NULL)
		return 1;

	
	if (results || self->initialized == 0)
	{
		count = setup_command_completion_data(self);

		if (count == -1)
			return 1;

		if (results)
			*results = count;

		self->results = count;

		self->initialized = 1;

		self->count = count;

		return 0;
	}

	if (result == NULL)
		return 0;

	if (get_result >= self->count)
		return 1;


	if (self->data == NULL)
		return 1;

	cc = self->data;

	s = NULL;
	switch (cc[get_result].type)
	{
		case 0:
			res = cc[get_result].info.c->name;
			t = "&cf55";
			break;
		case 1:
			res = cc[get_result].info.a->name;
			t = "&c5f5";
			s = cc[get_result].info.a->value;
			break;
		case 2:
			res = cc[get_result].info.v->name;
			t = "&cff5";
			s = cc[get_result].info.v->string;
			break;
		default:
			*result = NULL;
			return 1;
	}

	if (result_type == 0)
	{
		*result = va("%s", res);
		snprintf(self->real_name, sizeof(self->real_name), "%s", res);
	}
	else
	{
		*result = va("%s%s%s", context_sensitive_tab_completion_color_coded_types.value ? t : "",res, s ? va("%s -> %s", context_sensitive_tab_completion_color_coded_types.value ? "&cfff" : "",  s): "");
	}

	return 0;
}

static int Command_Completion_Get_Data(struct cst_info *self, int remove)
{
	if (remove)
	{
		free(self->data);
		self->data = NULL;
	}
	return 0;
}

int weight_disable = 1;

void Weight_Disable_f(void)
{
	weight_disable = 1;
}

void Weight_Enable_f(void)
{
	weight_disable = 0;
}

void Weight_Set_f(void)
{
	cvar_t *cvar;
	cmd_function_t *cmd_function;
	cmd_alias_t *cmd_alias;

	if (Cmd_Argc() != 3)
	{
		Com_Printf("Usage: %s [variable/command/alias] [weight].\n", Cmd_Argv(0));
		return;
	}

	if ((cvar=Cvar_FindVar(Cmd_Argv(2))))
	{
		cvar->weight = atoi(Cmd_Argv(1));
		return;
	}

	if ((cmd_function=Cmd_FindCommand(Cmd_Argv(2))))
	{
		cmd_function->weight = atoi(Cmd_Argv(1));
		return;
	}

	if ((cmd_alias=Cmd_FindAlias(Cmd_Argv(2))))
	{
		cmd_alias->weight = atoi(Cmd_Argv(1));
		return;
	}
}

static int Player_Color_Selector_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%i %i", self->color[0], self->color[1]);
		return 0;
	}

	return 1;
}

static int Color_Selector_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%i", get_result);
		return 0;
	}
	return 1;
}

static int Slider_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%f", self->slider_value);
		return 0;
	}
	return 1;
}

void Context_Sensitive_Tab_Completion_CvarInit(void)
{
	Command_Completion.name = "Command_Completion";
	Command_Completion.result = &Command_Completion_Result;
	Command_Completion.get_data = &Command_Completion_Get_Data;
	Command_Completion.conditions = NULL;
	Command_Completion.flags = CSTC_COMMAND;
	Cvar_Register(&context_sensitive_tab_completion);
	Cvar_Register(&context_sensitive_tab_completion_command_only_on_ctrl_tab);
	Cvar_Register(&context_sensitive_tab_completion_color_coded_types);
	Cvar_Register(&context_sensitive_tab_completion_close_on_tab);
	Cvar_Register(&context_sensitive_tab_completion_sorting_method);
	Cvar_Register(&context_sensitive_tab_completion_show_results);
	Cvar_Register(&context_sensitive_tab_completion_ignore_alt_tab);
	Cvar_Register(&context_sensitive_tab_completion_background_color);
	Cvar_Register(&context_sensitive_tab_completion_selected_color);
	Cvar_Register(&context_sensitive_tab_completion_inputbox_color);
	Cvar_Register(&context_sensitive_tab_completion_tooltip_color);
	Cvar_Register(&context_sensitive_tab_completion_highlight_color);
	Cvar_Register(&context_sensitive_tab_completion_insert_slash);
	Cvar_Register(&context_sensitive_tab_completion_slider_no_offset);
	Cvar_Register(&context_sensitive_tab_completion_slider_border_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_background_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_variables);
	Cvar_Register(&context_sensitive_tab_completion_execute_on_enter);
	Cmd_AddCommand("weight_enable", Weight_Enable_f);
	Cmd_AddCommand("weight_disable", Weight_Disable_f);
	Cmd_AddCommand("weight_set", Weight_Set_f);
	CC_Slider.result = &Slider_Result;
	CC_Slider.flags = CSTC_SLIDER | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Slider.tooltip = cstc_slider_tooltip;
	CC_Player_Color.result = &Player_Color_Selector_Result;
	CC_Player_Color.flags = CSTC_PLAYER_COLOR_SELECTOR | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Player_Color.tooltip = &cstc_player_color_tooltip;

	CC_Color.result = &Color_Selector_Result;
	CC_Color.flags = CSTC_COLOR_SELECTOR | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Color.tooltip = cstc_color_tooltip;

}

void Context_Weighting_Init(void)
{
	Cbuf_AddText("weight_disable\n");
	Cbuf_AddText("exec weight_file\n");
	Cbuf_AddText("weight_enable\n");
}

void CSTC_PictureInit(void)
{
	if (cstc_pictures)
		Draw_FreePicture(cstc_pictures);
	cstc_pictures = Draw_LoadPicture("gfx/cstc_pics.png", DRAW_LOADPICTURE_NOFALLBACK);
}

void CSTC_PictureShutdown(void)
{
	if (cstc_pictures)
	{
		Draw_FreePicture(cstc_pictures);
		cstc_pictures = NULL;
	}
}

void Context_Weighting_Shutdown(void)
{
	extern cvar_t *cvar_vars;
	extern cmd_function_t *cmd_functions;
	extern cmd_alias_t *cmd_alias;
	cmd_function_t *cmd;
	cmd_alias_t *alias;
	cvar_t *var;
	FILE *f;

	f = fopen("fodquake/weight_file", "w");

	if (f == NULL)
		return;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
		if (cmd->weight > 0)
			fprintf(f, "weight_set %i %s\n", cmd->weight, cmd->name);

	for (var=cvar_vars; var; var=var->next)
		if (var->weight > 0)
			fprintf(f, "weight_set %i %s\n", var->weight, var->name);

	for (alias=cmd_alias; alias; alias=alias->next)
		if (alias->weight > 0)
			fprintf(f, "weight_set %i %s\n", alias->weight, alias->name);

	fclose (f);
}

void CSTC_Console_Close(void)
{
	CSTC_Cleanup(cst_info);
}

void CSTC_Shutdown(void)
{
	struct cst_commands *c, *cc;

	c = commands;
	while (c)
	{
		cc = c->next;
		if (c->name)
			free(c->name);
		if (c->commands)
			Tokenize_String_Delete(c->commands);
		free(c);
		c = cc;
	}
}
