#include <mach/mach_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static mach_timebase_info_data_t tbinfo;

static unsigned int secbase;

static unsigned long long monotonictime()
{
	unsigned long long curtime;

	curtime = mach_absolute_time();

	curtime *= tbinfo.numer;
	curtime /= tbinfo.denom;

	return curtime;
}

double Sys_DoubleTime(void)
{
	return (double)monotonictime() / 1000000000.0;
}

unsigned long long Sys_IntTime()
{
	return monotonictime() / 1000;
}

char *Sys_ConsoleInput(void)
{
	return 0;
}

void Sys_Printf(char *fmt, ...)
{
}

void Sys_Quit(void)
{
	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list va;
	char string[1024];

	va_start(va, error);
	vsnprintf(string, sizeof(string), error, va);
	va_end(va);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();

	exit(1);
}

void Sys_CvarInit(void)
{
}

void Sys_Init(void)
{
}

int main(int argc, char **argv)
{
	double time, oldtime, newtime;

	mach_timebase_info(&tbinfo);

	COM_InitArgv(argc, argv);
	Host_Init(argc, argv);

	oldtime = Sys_DoubleTime();
	while(1)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);
	}

	return 0;
}

