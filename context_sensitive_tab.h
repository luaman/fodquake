
#define INPUT_MAX 512
struct cst_info
{
	char *name;
	char input[INPUT_MAX];
	int input_position;
	int selection;
	int direction;
	int argument_start;
	int argument_length;
	int results;
	int insert_space;
	struct tokenized_string *tokenized_input;

	// internal use of result and get_data
	qboolean *checked;
	int initialized;
	int count;
	void *data;
	

	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);

	int parser_behaviour;
};

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), int parser_behaviour);
void CSTC_Insert_And_Close(void);
void Context_Sensitive_Tab_Completion_Draw(void);
void Context_Sensitive_Tab_Completion_Key(int key);
int Context_Sensitive_Tab_Completion(void);
