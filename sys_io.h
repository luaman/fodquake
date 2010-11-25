enum directory_entry_type
{
	et_file,
	et_dir,
	et_false
};

struct directory_entry_temp
{
	struct directory_entry_temp *next;
	enum directory_entry_type type;
	char *name;
};

int Sys_Read_Dir(char *dir, char *subdir, int *gcount, struct directory_entry_temp **list, struct directory_entry_temp *(*add_det)(struct directory_entry_temp *tmp));


