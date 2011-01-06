#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "context_sensitive_tab.h"
#include "tokenize_string.h"

struct cst_commands
{
	struct cst_commands *next;
	char *name;
	int (*conditions)(void);
	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
	int parser_behaviour;
};

struct cst_commands Command_Completion;

#define MAXCMDLINE 256
extern int key_linepos;
extern int edit_line;
extern char key_lines[32][MAXCMDLINE];

int context_sensitive_tab_completion_active = 0;

cvar_t	context_sensitive_tab_completion = {"context_sensitive_tab_completion", "1"};

cvar_t	context_sensitive_tab_completion_execute_on_enter = {"context_sensitive_tab_completion_execute_on_enter", "1"};

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

	//free(info);
}

static struct cst_info cst_info_static;
static struct cst_info *cst_info = &cst_info_static;

static struct cst_commands *commands;

static void CSTC_Cleanup(struct cst_info *self)
{
	context_sensitive_tab_completion_active = 0;
	cleanup_cst(self);
}


void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), int parser_behaviour)
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
		snprintf(new_keyline, MAXCMDLINE, "%*.*s %s%s", self->argument_start, self->argument_start, key_lines[edit_line], result, key_lines[edit_line] + self->argument_start + self->argument_length);
	else
		snprintf(new_keyline, MAXCMDLINE, "%*.*s%s%s", self->argument_start, self->argument_start, key_lines[edit_line], result, key_lines[edit_line] + self->argument_start + self->argument_length);
	memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);

	key_linepos = self->argument_start + strlen(result);
	if (self->insert_space)
		key_linepos++;

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

void Context_Sensitive_Tab_Completion_Key(int key)
{
	int i;
	char *ptr;

	if (context_sensitive_tab_completion_active == 0)
		return;

	i = cst_info->results;

	if (key == K_ESCAPE)
	{
		CSTC_Cleanup(cst_info);
		return;
	}

	if (key == K_ENTER)
	{
		insert_result(cst_info, NULL);
		CSTC_Cleanup(cst_info);
		return;
	}

	if (key == K_BACKSPACE)
	{
		cst_info->input_position--;
		if (cst_info->input_position < 0)
			cst_info->input_position = 0;
		cst_info->input[cst_info->input_position] = '\0';
		Tokenize_Input(cst_info);
		cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
		return;
	}

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

	if (key == K_TAB)
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

	if ((key > 32 && key < 127) || key == K_SPACE) 
	{
#warning So the \0 will be inserted at buffer + INPUT_MAX? Buffer overflow.
		if (cst_info->input_position >= INPUT_MAX-1)
		{
			cst_info->input_position = INPUT_MAX - 2;
			return;
		}
		cst_info->input[cst_info->input_position++] = key;
		cst_info->input[cst_info->input_position] = '\0';
		Tokenize_Input(cst_info);
		cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
	}

}

static void CSTC_Draw(struct cst_info *self, int y_offset)
{
	int i, result_offset, offset, rows, sup, sdown;
	char *ptr;

	if (self->direction == -1)
		offset = y_offset - 32;
	else
		offset = y_offset - 14;

	Draw_Fill(0, offset , vid.width, 10, 4);
	Draw_String(8, offset, self->input);
	Draw_String(8 + self->input_position * 8 , offset + 2, "_");

	offset += 8 * self->direction;

	if (self->direction == -1)
		rows = offset / 8;
	else
		rows = (vid.height - offset) / 8;

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

	for (i=0, ptr = NULL; i<rows; i++)
	{
		if (self->result(self, NULL, i + result_offset, 1, &ptr))
			break;
		if (i + result_offset == self->selection)
			Draw_Fill(0, offset + i * 8 * self->direction, vid.width, 8, 40);
		else
			Draw_Fill(0, offset + i * 8 * self->direction, vid.width, 8, 4);

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
}

void Context_Sensitive_Tab_Completion_Draw(void)
{
	extern float scr_conlines;

	if (context_sensitive_tab_completion_active == 0)
		return;

	if (cst_info == NULL)
		return;

	if (scr_conlines > vid.height/2)
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

	*start = s;

	while(*s && *s != endchar && (endchar != ' ' || *s != ';'))
		s++;

	*end = s;

	if (*s == '"')
		s++;

	*next = s;
}

void read_info_new (char *string, int position, char **cmd_begin, int *cmd_len, char **arg_begin, int *arg_len, int *cursor_on_command, int *is_invalid)
{
	char *cmd_start, *cmd_stop, *arg_start, *arg_stop;
	char **start, **stop;
	char *s;
	char *next;
	unsigned int i;
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
	c->result = cc->result;
	c->get_data = cc->get_data;
	snprintf(c->input, sizeof(c->input), "%.*s", arg_len, key_lines[edit_line] + arg_start);
	c->input_position = arg_len;
	Tokenize_Input(c);
	c->argument_start = arg_start;
	c->argument_length = arg_len;
	if (c->get_data)
		c->get_data(c, 0);
	c->result(c, &c->results, 0, 0, NULL);
	c->insert_space = insert_space;
	c->parser_behaviour = cc->parser_behaviour;
}

static int setup_current_command(void)
{
	int cmd_len, arg_len, cursor_on_command, isinvalid;
	char *cmd_start, *arg_start;
	struct cst_commands *c;

		
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

		while (c)
		{
			if (strncmp(c->name, cmd_start, cmd_len) == 0)
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
				return 1;
			}
			c = c->next;
		}
	}
	return 0;
}

int Context_Sensitive_Tab_Completion(void)
{
	if (setup_current_command())
	{
		context_sensitive_tab_completion_active = 1;
		Com_Printf("Context sensitive active for %s\n", cst_info->name);
		return 1;
	}

	Com_Printf("Context sensitive not active\n");

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

static int setup_command_completion_data(struct cst_info *self)
{
	extern cvar_t *cvar_vars;
	extern cmd_function_t *cmd_functions;
	extern cmd_alias_t *cmd_alias;
	cmd_function_t *cmd;
	cmd_alias_t *alias;
	cvar_t *var;
	int count, i, add;

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
		if (self->tokenized_input->count == 1)
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
		if (self->tokenized_input->count == 1)
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

		if (self->tokenized_input->count == 1)
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
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (strstr(cmd->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
		{
			cd->info.c = cmd;
			cd->type = 0;
			cd++;
		}
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (strstr(alias->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}

		if (add)
		{
			cd->info.a = alias;
			cd->type = 1;
			cd++;
		}
	}

	for (var=cvar_vars; var; var=var->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (strstr(var->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
		{
			cd->info.v = var;
			cd->type = 2;
			cd++;
		}
	}

	cd = self->data;

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
		*result = va("/%s", res);
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

void Context_Sensitive_Tab_Completion_CvarInit(void)
{
	Command_Completion.name = "Command_Completion";
	Command_Completion.result = &Command_Completion_Result;
	Command_Completion.get_data = &Command_Completion_Get_Data;
	Command_Completion.conditions = NULL;
	Cvar_Register(&context_sensitive_tab_completion);
}

