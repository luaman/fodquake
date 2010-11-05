#warning No. Wrong place for this prototype.
#warning The best way to implement this would be to make Sys_Read_Dir() take a callback function that gets called for each entry in the directory.
struct directory_entry_temp *add_det(struct directory_entry_temp **list);
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

int Sys_Read_Dir (char *dir, char *subdir, int *gcount, struct directory_entry_temp **list);


