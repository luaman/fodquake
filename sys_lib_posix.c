/*
Copyright (C) 2011 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <dlfcn.h>

#include <stdlib.h>
#include <stdio.h>

#include "sys_lib.h"

struct SysLib
{
	void *elf_handle;
};

struct SysLib *Sys_Lib_Open(const char *libname)
{
	struct SysLib *lib;
	char newname[256];

	lib = malloc(sizeof(*lib));
	if (lib)
	{
		snprintf(newname, sizeof(newname), "lib%s.so", libname);

		lib->elf_handle = dlopen(newname, RTLD_NOW);
		if (lib->elf_handle)
		{
			return lib;
		}

		free(lib);
	}

	return 0;
}

void Sys_Lib_Close(struct SysLib *lib)
{
	dlclose(lib->elf_handle);
	free(lib);
}

void *Sys_Lib_GetAddressByName(struct SysLib *lib, const char *symbolname)
{
	return dlsym(lib->elf_handle, symbolname);
}

