#include <proto/dos.h>

#include "sys_io.h"

int Sys_Read_Dir(char *dir, char *subdir, int *gcount, struct directory_entry_temp **list, struct directory_entry_temp *(*add_det)(struct directory_entry_temp **tmp))
{
	return 1;
}

void Sys_IO_Create_Directory(const char *path)
{
	BPTR lock;

	lock = CreateDir(path);
	if (lock)
	{
		UnLock(lock);
	}
}

