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

