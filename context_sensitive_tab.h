#define CSTC_NO_INPUT			( 1 << 0)
#define CSTC_COLOR_SELECTOR		( 1 << 1)
#define CSTC_MULTI_COMMAND		( 1 << 2)

#define INPUT_MAX 512
struct cst_info
{
	char *name;
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

	// internal use of result and get_data
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
