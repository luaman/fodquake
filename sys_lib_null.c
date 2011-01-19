#include "sys_lib.h"

struct SysLib
{
};

struct SysLib *Sys_Lib_Open(const char *libname)
{
	return 0;
}

void Sys_Lib_Close(struct SysLib *lib)
{
}

void *Sys_Lib_GetAddressByName(struct SysLib *lib, const char *symbolname)
{
	return 0;
}

