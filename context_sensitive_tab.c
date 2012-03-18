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


struct cst_commands
{
	struct cst_commands *next;
	char *name;
	struct tokenized_string *commands;
	int (*conditions)(void);
	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
	int parser_behaviour;
	int flags;
};

struct cst_commands Command_Completion;

#define MAXCMDLINE 256
extern int key_linepos;
extern int edit_line;
extern char key_lines[32][MAXCMDLINE];

int context_sensitive_tab_completion_active = 0;

cvar_t	context_sensitive_tab_completion = {"context_sensitive_tab_completion", "1"};
cvar_t	context_sensitive_tab_completion_close_on_tab = {"context_sensitive_tab_completion_close_on_tab", "1"};
cvar_t	context_sensitive_tab_completion_sorting_method = {"context_sensitive_tab_completion_sorting_method", "1"};
cvar_t	context_sensitive_tab_completion_show_results = {"context_sensitive_tab_completion_show_results", "1"};
cvar_t	context_sensitive_tab_completion_ignore_alt_tab = {"context_sensitive_tab_completion_ignore_alt_tab", "1"};
cvar_t	context_sensitive_tab_completion_background_color = {"context_sensitive_tab_completion_background_color", "4"};
cvar_t	context_sensitive_tab_completion_inputbox_color = {"context_sensitive_tab_completion_inputbox_color", "4"};
cvar_t	context_sensitive_tab_completion_selected_color = {"context_sensitive_tab_completion_selected_color", "40"};
cvar_t	context_sensitive_tab_completion_insert_slash = {"context_sensitive_tab_completion_insert_slash", "1"};
cvar_t	context_sensitive_tab_completion_slider_no_offset = {"context_sensitive_tab_completion_slider_no_offset", "1"};
cvar_t	context_sensitive_tab_completion_slider_border_color = {"context_sensitive_tab_completion_slider_border_color", "0"};
cvar_t	context_sensitive_tab_completion_slider_background_color = {"context_sensitive_tab_completion_slider_background_color", "4"};
cvar_t	context_sensitive_tab_completion_slider_color = {"context_sensitive_tab_completion_slider_color", "10"};

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

static void CSTC_Cleanup(struct cst_info *self)
{
	context_sensitive_tab_completion_active = 0;
	cleanup_cst(self);
	memset(self, 0, sizeof(struct cst_info));
}

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), int parser_behaviour, int flags)
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
	command->parser_behaviour = parser_behaviour;
	command->flags = flags;
	if (flags & CSTC_MULTI_COMMAND)
		command->commands = Tokenize_String(command->name);
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

	if (ptr)
		result = ptr;
	else
		if (cst_info->result(cst_info, NULL, cst_info->selection, 0, &result))
			return;

	if (self->insert_space)
		snprintf(new_keyline, MAXCMDLINE, "%*.*s %s %s", self->argument_start, self->argument_start, key_lines[edit_line], result, key_lines[edit_line] + self->argument_start + self->argument_length);
	else
		if (context_sensitive_tab_completion_insert_slash.value == 1 && self->argument_start == 1 && key_lines[edit_line][1] != '/')
			snprintf(new_keyline, MAXCMDLINE, "%*.*s/%s %s", self->argument_start, self->argument_start, key_lines[edit_line], result, key_lines[edit_line] + self->argument_start + self->argument_length);
		else
			snprintf(new_keyline, MAXCMDLINE, "%*.*s%s %s", self->argument_start, self->argument_start, key_lines[edit_line], result, key_lines[edit_line] + self->argument_start + self->argument_length);
	memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);

	key_linepos = self->argument_start + strlen(result);

	if (self->insert_space)
		key_linepos++;

	key_linepos++;

	if (key_linepos >= MAXCMDLINE)
		key_linepos = MAXCMDLINE - 1;

#warning You need to clamp key_linepos here (it can otherwise overflow)
}

void CSTC_Insert_And_Close(void)
{
	insert_result(cst_info, NULL);
	context_sensitive_tab_completion_active = 0;
}

static void cstc_insert_only_find(struct cst_info *self)
{
	insert_result(self, NULL);
	CSTC_Cleanup(self);
}

static int valid_color(int i)
{
	if (i < 8)
		return i * 16 + 15;
	else
		return i *16;
}

void Context_Sensitive_Tab_Completion_Key(int key)
{
	int i;

	if (context_sensitive_tab_completion_active == 0)
		return;

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

	if (key == K_ENTER)
	{
		insert_result(cst_info, NULL);
		CSTC_Cleanup(cst_info);
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
	int i, result_offset, offset, rows, sup, sdown, x, y, pos_x, pos_y, sp_x[3], sp_y[3];
	char *ptr;
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
					sp_x[2] = pos_x;
					sp_y[2] = pos_y;
				}
			}
		}
		Draw_String(sp_x[2]  , sp_y[2] , "x");
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
			if (self->result(self, NULL, i + result_offset, 1, &ptr))
				break;
			if (i + result_offset == self->selection)
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_selected_color.value);
			else
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_background_color.value);

			Draw_String(32, offset + i * 8 * self->direction, ptr);
		}

		if (sup)
		{
			Draw_String(8, offset + (rows - 1) * 8 * self->direction, "^");
		}

		if (sdown)
		{
			Draw_String(8, offset + 8 * self->direction, "v");
		}

		if (rows < self->results && context_sensitive_tab_completion_show_results.value == 1)
		{
			sprintf(buf, "showing %i of %i results", rows, self->results);
			Draw_String(vid.conwidth - strlen(buf) * 8, offset, buf);
		}
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

static void getarg(const char *s, char **start, char **end, char **next)
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
		endchar = ' ';

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
			getarg(s, start, stop, &next);

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

static void setup_completion(struct cst_commands *cc, struct cst_info *c, int arg_start, int arg_len, int insert_space)
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
	if (c->get_data)
		c->get_data(c, 0);
	c->result(c, &c->results, 0, 0, NULL);
	c->insert_space = insert_space;
	c->parser_behaviour = cc->parser_behaviour;
	c->flags = cc->flags;
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
	struct cst_commands *c;
	cvar_t *var;
	char new_keyline[MAXCMDLINE];

	read_info_new(key_lines[edit_line] + 1, key_linepos, &cmd_start, &cmd_len, &arg_start, &arg_len, &cursor_on_command, &isinvalid);

	if (isinvalid)
		return 0;

	if (cursor_on_command || key_lines[edit_line] + key_linepos == cmd_start + cmd_len)
	{
		setup_completion(&Command_Completion, cst_info, cmd_start - key_lines[edit_line], cmd_len, 0);
		return 1;
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
						break;
					}
				}
			}
			else
			{
				if (cmd_len == strlen(c->name) && strncasecmp(c->name, cmd_start, cmd_len) == 0)
				{
					name = c->name;
					break;
				}
			}

			if (dobreak)
				break;
			c = c->next;
		}

		if (c)
		{
			if (c->conditions)
				if (c->conditions() == 0)
					return 0;
			if (arg_start - key_lines[edit_line] - 1 == 0)
				setup_completion(c, cst_info, cmd_start + cmd_len - key_lines[edit_line], arg_len, 1);
			else
			{
				if (c->parser_behaviour == 0)
					setup_completion(c, cst_info, arg_start - key_lines[edit_line], arg_len, 0);
				else
				{
					setup_completion(c, cst_info, cmd_start + cmd_len - key_lines[edit_line] + 1,  (arg_start - cmd_start) + arg_len-1, 0);
				}
			}
			cst_info->function = Cmd_FindCommand(name);
			cst_info->variable = Cvar_FindVar(name);
			if (cst_info->flags & CSTC_SLIDER)
				setup_slider(cst_info);
			return 1;
		}
		else
		{
			snprintf(new_keyline, MAXCMDLINE, "%*.*s", cmd_len, cmd_len, cmd_start);
			var = Cvar_FindVar(new_keyline);
			if (var)
			{
				snprintf(new_keyline, MAXCMDLINE, "%s\"%s\"", key_lines[edit_line], var->string);
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
		if (self->tokenized_input->count == 1 && 0)
		{
			if (strcmp(cmd->name, self->tokenized_input->tokens[0]) == 0)
			{
				cd = self->data = calloc(1, sizeof(struct cva_s));
				if (cd == NULL)
					return -1;
				cd->type = 0;
				cd->info.c = cmd;
				return 1;
			}
		}
		{
			for (i=0; i<self->tokenized_input->count; i++)
			{
				if (strstr(cmd->name, self->tokenized_input->tokens[i]) == NULL)
				{
					add = 0;
					break;
				}
			}
			if (add)
				count++;
		}
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		add = 1;
		if (self->tokenized_input->count == 1 && 0)
		{
			if (strcmp(alias->name, self->tokenized_input->tokens[0]) == 0)
			{
				cd = self->data = calloc(1, sizeof(struct cva_s));
				if (cd == NULL)
					return -1;
				cd->type = 1;
				cd->info.a = alias;
				return 1;
			}
		}
		{
			for (i=0; i<self->tokenized_input->count; i++)
			{
				if (strstr(alias->name, self->tokenized_input->tokens[i]) == NULL)
				{
					add = 0;
					break;
				}
			}
			if (add)
				count++;
		}
	}

	for (var=cvar_vars; var; var=var->next)
	{
		add = 1;

		if (self->tokenized_input->count == 1 && 0)
		{
			if (strcmp(var->name, self->tokenized_input->tokens[0]) == 0)
			{
				cd = self->data = calloc(1, sizeof(struct cva_s));
				if (cd == NULL)
					return -1;
				cd->type = 2;
				cd->info.v = var;
				return 1;
			}
		}
		{
			for (i=0; i<self->tokenized_input->count; i++)
			{
			
				if (strstr(var->name, self->tokenized_input->tokens[i]) == NULL)
				{
					add = 0;
					break;
				}
			}
			if (add)
				count++;
		}
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
			if ((s = strstr(cmd->name, self->tokenized_input->tokens[i])) == NULL)
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
			if ((s = strstr(alias->name, self->tokenized_input->tokens[i])) == NULL)
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
			if ((s = strstr(var->name, self->tokenized_input->tokens[i])) == NULL)
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
	else
		qsort(self->data, count, sizeof(struct cva_s), &name_compare);

	return count;
}

static int Command_Completion_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	int count;
	struct cva_s *cc;
	char *res;

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

	switch (cc[get_result].type)
	{
		case 0:
			res = cc[get_result].info.c->name;
			break;
		case 1:
			res = cc[get_result].info.a->name;
			break;
		case 2:
			res = cc[get_result].info.v->name;
			break;
		default:
			*result = NULL;
			return 1;
	}

	if (result_type == 0)
		*result = va("%s", res);
	else
		*result = res;

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
	Cvar_Register(&context_sensitive_tab_completion);
	Cvar_Register(&context_sensitive_tab_completion_close_on_tab);
	Cvar_Register(&context_sensitive_tab_completion_sorting_method);
	Cvar_Register(&context_sensitive_tab_completion_show_results);
	Cvar_Register(&context_sensitive_tab_completion_ignore_alt_tab);
	Cvar_Register(&context_sensitive_tab_completion_background_color);
	Cvar_Register(&context_sensitive_tab_completion_selected_color);
	Cvar_Register(&context_sensitive_tab_completion_inputbox_color);
	Cvar_Register(&context_sensitive_tab_completion_insert_slash);
	Cvar_Register(&context_sensitive_tab_completion_slider_no_offset);
	Cvar_Register(&context_sensitive_tab_completion_slider_border_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_background_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_color);
	Cmd_AddCommand("weight_enable", Weight_Enable_f);
	Cmd_AddCommand("weight_disable", Weight_Disable_f);
	Cmd_AddCommand("weight_set", Weight_Set_f);
	CSTC_Add("enemycolor teamcolor", NULL, &Player_Color_Selector_Result, NULL, 0, CSTC_PLAYER_COLOR_SELECTOR | CSTC_MULTI_COMMAND | CSTC_NO_INPUT);
	CSTC_Add("context_sensitive_tab_completion_inputbox_color context_sensitive_tab_completion_selected_color context_sensitive_tab_completion_background_color sb_color_bg sb_color_bg_empty sb_color_bg_free sb_color_bg_specable sb_color_bg_full sb_highlight_sort_column_color topcolor bottomcolor r_skycolor context_sensitive_tab_completion_slider_border_color context_sensitive_tab_completion_slider_background_color context_sensitive_tab_completion_slider_color", NULL, &Color_Selector_Result, NULL, 0, CSTC_COLOR_SELECTOR | CSTC_MULTI_COMMAND);
	CSTC_Add("volume gl_gamma", NULL, &Slider_Result, NULL, 0, CSTC_SLIDER | CSTC_NO_INPUT | CSTC_MULTI_COMMAND);
}

void Context_Weighting_Init(void)
{
	Cbuf_AddText("weight_disable\n");
	Cbuf_AddText("exec weight_file\n");
	Cbuf_AddText("weight_enable\n");
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

