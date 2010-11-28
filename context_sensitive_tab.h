
#define INPUT_MAX 512
struct cst_info
{
	char *name;
	char input[INPUT_MAX];
	int input_position;
	int selection;
	int direction;
	int argument_start;
	int results;
	struct tokenized_string *tokenized_input;

	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);

	void *data;
};

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove));
void CSTC_Insert_And_Close(char *result, int argument_start);
void Context_Sensitive_Tab_Completion_Draw(void);
void Context_Sensitive_Tab_Completion_Key(int key);
int Context_Sensitive_Tab_Completion(void);
