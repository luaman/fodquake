#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "sys_io.h"

#include "windows.h"

int Sys_Read_Dir(char *dir, char *subdir, int *gcount, struct directory_entry_temp **list, struct directory_entry_temp *(*add_det)(struct directory_entry_temp **tmp))
{
	enum directory_entry_type type;
	char dir_buf[4096];
	char file_buf[4096];
	char dir_buf_temp[4096];
	struct directory_entry_temp *detc;
	int count;

	HANDLE h;
	WIN32_FIND_DATA fd;

	if (dir == NULL || list == NULL)
		return 1;

	if (subdir == NULL)
		snprintf(dir_buf, sizeof(dir_buf), "%s/", dir);
	else
		snprintf(dir_buf, sizeof(dir_buf), "%s/%s/", dir, subdir);

	snprintf(dir_buf_temp, sizeof(dir_buf_temp), "%s/*.*", dir_buf);
	h = FindFirstFile(dir_buf_temp, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 1;

	count = 0;
	do
	{

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			type = et_dir;
			if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;
		}
		else
		{
			type = et_file;
		}

		detc = add_det(list);
		if (detc == NULL)
		{
			FindClose(h);
			return 1;
		}

		detc->type = type;

		if (subdir)
			snprintf(file_buf, sizeof(file_buf), "%s/%s", subdir, fd.cFileName);
		else
			snprintf(file_buf, sizeof(file_buf), "%s", fd.cFileName);

		detc->name = strdup(file_buf);
		if (detc->name == NULL)
		{
			FindClose(h);
			return 1;
		}
		count++;

	} while (FindNextFile(h, &fd));
	FindClose(h);

	if (gcount)
		*gcount = *gcount + count;

	return 0;
}

void Sys_IO_Create_Directory(const char *path)
{
	CreateDirectory(path, 0);
}

int Sys_IO_Path_Exists(const char *path)
{
	DWORD attributes;

	attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES;
}

int Sys_IO_Path_Writable(const char *path)
{
	DWORD attributes;

	attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_READONLY);
}

