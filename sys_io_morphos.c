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

int Sys_IO_Path_Exists(const char *path)
{
	BPTR lock;

	lock = Lock(path, ACCESS_READ);
	if (lock)
		UnLock(lock);

	return !!lock;
}

int Sys_IO_Path_Writable(const char *path)
{
	struct InfoData id;
	BPTR lock;
	int ret;

	ret = 0;

	lock = Lock(path, ACCESS_READ);
	if (lock)
	{
		if (Info(lock, &id))
		{
			if (id.id_DiskState == ID_VALIDATED)
				ret = 1;
		}

		UnLock(lock);
	}

	return !!lock;
}

