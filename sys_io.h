#ifndef SYS_IO_H
#define SYS_IO_H

enum directory_entry_type
{
	et_file,
	et_dir,
};

struct directory_entry
{
	char *name;
	unsigned long long size;
	enum directory_entry_type type;
};


void Sys_IO_Create_Directory(const char *path);

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque);

int Sys_IO_Path_Exists(const char *path);
int Sys_IO_Path_Writable(const char *path);

#endif /* SYS_IO_H */

