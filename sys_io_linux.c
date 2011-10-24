#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "sys_io.h"

#include "dirent.h"
#include "sys/stat.h"

void Sys_IO_Create_Directory(const char *path)
{
	mkdir(path, 0777);
}

int Sys_Read_Dir(char *dir, char *subdir, int *gcount, struct directory_entry_temp **list, struct directory_entry_temp *(*add_det)(struct directory_entry_temp **tmp))
{
	enum directory_entry_type type;
	char dir_buf[4096];
	char file_buf[4096];
	struct directory_entry_temp *detc;
	int count;

	DIR *DIR_dir;
	struct dirent *dirent;
	struct stat fileinfo;


	if (dir == NULL || list == NULL)
		return 1;


	if (subdir == NULL)
		snprintf(dir_buf, sizeof(dir_buf), "%s/", dir);
	else
		snprintf(dir_buf, sizeof(dir_buf), "%s/%s/", dir, subdir);

	DIR_dir = opendir(dir_buf);
	
	if (DIR_dir == NULL)
		return 1;

	dirent = readdir(DIR_dir);
	if (dirent == NULL)
#warning leaks DIR_dir
		return 1;

	count = 0;
	do
	{
		snprintf(file_buf, sizeof(file_buf), "%s/%s", dir_buf, dirent->d_name);
		if (stat(file_buf, &fileinfo) < 0)
		{
			closedir(DIR_dir);
			return 1;
		}

		if (S_ISDIR(fileinfo.st_mode))
		{
			if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
				continue;

			type = et_dir;
		}
		else
		{
			type = et_file;
		}

		detc = add_det(list);
		if (detc == NULL)
		{
			closedir(DIR_dir);
			return 1;
		}

		detc->type = type;

		if (subdir)
			snprintf(file_buf, sizeof(file_buf), "%s/%s", subdir, dirent->d_name);
		else
			snprintf(file_buf, sizeof(file_buf), "%s", dirent->d_name);

		detc->name = strdup(file_buf);
		if (detc->name == NULL)
		{
			closedir(DIR_dir);
			return 1;
		}
		count++;
	} while ((dirent = readdir(DIR_dir)));
	closedir(DIR_dir);

	if (gcount)
		*gcount = *gcount + count;

	return 0;
}

int Sys_IO_Path_Exists(const char *path)
{
	return access(path, F_OK) == 0;
}

int Sys_IO_Path_Writable(const char *path)
{
	return access(path, W_OK) == 0;
}

