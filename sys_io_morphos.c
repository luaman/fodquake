#include <proto/dos.h>

#include "sys_io.h"

void Sys_IO_Create_Directory(const char *path)
{
	BPTR lock;

	lock = CreateDir(path);
	if (lock)
	{
		UnLock(lock);
	}
}

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque)
{
	return 0;
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

