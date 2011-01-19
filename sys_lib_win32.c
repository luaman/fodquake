#include <windows.h>

#include "sys_lib.h"

struct SysLib
{
	HINSTANCE *dll_handle;
};

struct SysLib *Sys_Lib_Open(const char *libname)
{
	struct SysLib *lib;
	char newname[256];

	lib = malloc(sizeof(*lib));
	if (lib)
	{
		snprintf(newname, "lib%s.dll", libname);

		lib->dll_handle = LoadLibrary("libpng.dll");
		if (lib->dll_handle)
		{
			return lib;
		}

		free(lib);
	}

	return 0;
}

void Sys_Lib_Close(struct SysLib *lib)
{
	FreeLibrary(lib->dll_handle);
	free(lib);
}

void *Sys_Lib_GetAddressByName(struct SysLib *lib, const char *symbolname)
{
	return GetProcAddress(lib, symbolname);
}

