#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "context_sensitive_tab.h"
#include "tokenize_string.h"

#warning "Try entering 'cfg_load                          j<tab>' in Quake, assuming you have a one and only one config that starts with a j. In my case it's jogi.cfg, and my command line ends up looking something like 'cfg_load jogi.cfg                 j'"

struct cst_commands
{
	struct cst_commands *next;
	char *name;
	int (*conditions)(void);
	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
};

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

	if (info->get_data)
		info->get_data(info, 1);

	free(info);
}

static struct cst_info *cst_info;

static struct cst_commands *commands;

static void CSTC_Cleanup(struct cst_info *self)
{
	context_sensitive_tab_completion_active = 0;
	cleanup_cst(self);
}


void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove))
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
}

static struct cst_info *create_info_from_command(struct cst_commands *cc, char *args)
{

	struct cst_info *info;

	if (cc == NULL)
		return NULL;

	info = (struct cst_info *) calloc(1, sizeof(struct cst_info));
	
	if (info == NULL)
		return NULL;

	if (args)
		snprintf(info->input, sizeof(info->input), "%s", args);

	info->name = cc->name;
	info->result = cc->result;
	info->get_data = cc->get_data;

	return info;
}

static void strip_leading_space(char *string, int size)
{
	int i;
	char *p1;

	if (string[0] != ' ')
		return;

	p1 = string;
	while (*p1 != '\0' && isspace(*p1))
		p1++;
	i = strlen(p1);
	memmove(string, p1, i + 1);

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

static struct cst_info *get_current_command(void)
{
	int cp;
	int ep;
	int i;
	struct cst_commands *cc;
	struct cst_info *ci;
	char cmd[512], info[512];

	cmd[0] = '\0';

	cp = key_linepos;

#warning Why the '/' here?
	while (cp > 1 && key_lines[edit_line][cp] != ';' && key_lines[edit_line][cp] != '/')
	{
		cp--;
	}

	if (key_lines[edit_line][cp] == ';' || key_lines[edit_line][cp] == '/')
	{
		cp++;
	}

#warning The following code makes 'foo; cfg_load <tab>' fail, where 'foo;cfg_load <tab>' works. I guess you want to skip whitespaces first?
	ep = 0;

	while ((ep + cp) < key_linepos)
	{
		if (isspace(key_lines[edit_line][ep + cp]))
			break;
		ep++;
	}
	ep--;

	if (ep == 0)
		return NULL;

#warning Potential buffer overflow here.
	for (i=0; i<=ep; i++)
		cmd[i] = key_lines[edit_line][cp+i];
	cmd[i] = '\0';

	info[0] = '\0';
	ep++;
	for (i=0; i<(key_linepos - (ep + cp)); i++)
		info[i] = key_lines[edit_line][ep+cp+i];	
	info[i] = '\0';

#warning How about just skipping the space before copying?
	strip_leading_space(info, strlen(info));

	cc = commands;

	while (cc)
	{
		if (strcmp(cc->name, cmd) == 0)
			break;
		cc = cc->next;
	}

	if (cc == NULL)
		return NULL;


	if (cc->conditions)
		if (cc->conditions() == 0)
			return NULL;

	ci = create_info_from_command(cc, info);
	ci->argument_start = ep + cp;
	ci->input_position = strlen(ci->input);

	Tokenize_Input(ci);

	if (ci->get_data)
	{
		if (ci->get_data(ci, 0))
		{
#warning Worst. Com_Printf(). Ever. :) Same for the next two.
			Com_Printf("get_data was 1\n");
			CSTC_Cleanup(ci);
			return NULL;
		}
	}

	if (ci->result(ci, &ci->results, 0, 0, NULL))
	{
		CSTC_Cleanup(ci);
		Com_Printf("results returned 1\n");
		return NULL;
	}

	if (ci->results == 0)
	{
		Com_Printf("results was 0\n");
		CSTC_Cleanup(ci);
		return NULL;
	}


	return ci;
}

static struct cst_info *check_is_command(void)
{
	struct cst_info *current_command;

	if (key_linepos < 2)
	{
		return NULL;
	}

	current_command = get_current_command();

	return current_command;
}

int Context_Sensitive_Tab_Completion(void)
{
	cst_info = check_is_command();

	if (cst_info)
	{
		context_sensitive_tab_completion_active = 1;
		Com_Printf("Context sensitive active for %s\n", cst_info->name);
		return 1;
	}

	Com_Printf("Context sensitive not active\n");

	return 0;
}

#warning This function definitely needs to be a bit more graceful. You need to move the rest of the command line, if any, to the right first. Also you should probably update the cursor position.
static void insert_result(char *result, int argument_start)
{
	int i;

	if (!result)
		return;

	key_lines[edit_line][argument_start] = ' ';
#warning Potential buffer overflow.
	for (i=0; i<strlen(result);i++)
		key_lines[edit_line][argument_start + 1 + i] = result[i];

}

void CSTC_Insert_And_Close(char *result, int argument_start)
{
	insert_result(result, argument_start);
	context_sensitive_tab_completion_active = 0;
}

static void cstc_insert_only_find(struct cst_info *self)
{
	char *ptr;
	self->result(self, NULL, 0, 0, &ptr);
	insert_result(ptr, self->argument_start);
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
		if (!cst_info->result(cst_info, NULL, cst_info->selection, 0, &ptr))
			insert_result(ptr, cst_info->argument_start);
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
		if (cst_info->input_position >= INPUT_MAX)
		{
			cst_info->input_position = INPUT_MAX - 1;
			return;
		}
		cst_info->input[cst_info->input_position++] = key;
		cst_info->input[cst_info->input_position] = '\0';
		Tokenize_Input(cst_info);
		cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
	}

}

static void CSTC_Draw(struct cst_info *self, int y_offset);
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


/*
static void SC_Test(void)
{
	struct directory_list *dlist;
	int i;

	//char *demo_endigs[] = {".mvd", ".qwd", ".qwz", NULL};
	//dlist = Util_Dir_Recursive_Read_Filter(va("%s",com_basedir), demo_endigs);

	dlist = Util_Dir_Recursive_Read_Filter("//Mephisto/tv", NULL);
	Com_Printf("%s\n", com_basedir);

	if (dlist == NULL)
	{
		Com_Printf("reading failed!\n");
		return;
	}

	Util_Dir_Sort(dlist);

	Com_Printf("%i\n", dlist->entry_count);

	for (i=0; i<dlist->entry_count; i++)
	{
		if (dlist->entries[i].type == et_dir)
			Com_Printf("DIR : %s\n", dlist->entries[i].name);
		else if (dlist->entries[i].type == et_file)
			Com_Printf("FILE: %s\n", dlist->entries[i].name);

	}
	Util_Dir_Delete(dlist);
}
*/

#warning "This function happily draws far outside the screen and there's also no scrolling offered."
static void CSTC_Draw(struct cst_info *self, int y_offset)
{
	int i, offset;
	char *ptr;

	if (self->direction == -1)
		offset = y_offset - 32;
	else
		offset = y_offset - 14;

	Draw_Fill(0, offset , vid.width, 10, 4);
	Draw_String(8, offset, self->input);
	Draw_String(8 + self->input_position * 8 , offset + 2, "_");

	offset += 8 * self->direction;

	for (i=0, ptr = NULL; i<self->results; i++)
	{
		if (self->result(self, NULL, i, 1, &ptr))
			break;
		if (i == self->selection)
			Draw_Fill(0, offset + i * 8 * self->direction, vid.width, 8, 40);
		else
			Draw_Fill(0, offset + i * 8 * self->direction, vid.width, 8, 4);

		Draw_String(32, offset + i * 8 * self->direction, ptr);
	}
}

#warning This should be CvarInit, and its calling point in cl_main.c should be changed.
void Context_Sensitive_Tab_Completion_Init(void)
{
	//Cmd_AddCommand("sc_test", &SC_Test);

	Cvar_Register(&context_sensitive_tab_completion);
}

