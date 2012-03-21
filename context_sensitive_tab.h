#define CSTC_NO_INPUT					( 1 << 0)
#define CSTC_COLOR_SELECTOR				( 1 << 1)
#define CSTC_PLAYER_COLOR_SELECTOR		( 1 << 2)
#define CSTC_MULTI_COMMAND				( 1 << 3)
#define CSTC_SLIDER						( 1 << 4)
#define CSTC_EXECUTE					( 1 << 5)

#define INPUT_MAX 512
struct cst_info
{
	char *name;
	char real_name[INPUT_MAX];	//for multicmd stuff
	struct tokenized_string *commands;
	char input[INPUT_MAX];
	int selection;
	int direction;
	int argument_start;
	int argument_length;
	int results;
	int insert_space;
	struct tokenized_string *tokenized_input;
	int flags;

	cmd_function_t *function;
	cvar_t *variable;

	// internal use of result and get_data
	double slider_value, slider_original_value;
	int color[2];
	qboolean *checked;
	int initialized;
	int count;
	void *data;
	

	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);

	int parser_behaviour;
	struct input *new_input;
};

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), int parser_behaviour, int flags);
void CSTC_Insert_And_Close(void);
void Context_Sensitive_Tab_Completion_CvarInit(void);
void Context_Weighting_Init(void);
void Context_Weighting_Shutdown(void);
void Context_Sensitive_Tab_Completion_Draw(void);
void Context_Sensitive_Tab_Completion_Key(int key);
int Context_Sensitive_Tab_Completion(void);
void CSTC_Console_Close(void);
void CSTC_Shutdown(void);
