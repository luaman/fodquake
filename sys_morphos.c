#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <time.h>

#include <signal.h>

#include "quakedef.h"

int __stack = 1024*1024*4;

struct Library *SocketBase;

int do_stdin = 0;
qboolean stdin_ready;

void Sys_Shutdown()
{
	if (SocketBase)
	{
		CloseLibrary(SocketBase);
		SocketBase = 0;
	}
}

void Sys_Init()
{
}

void Sys_Quit()
{
	Sys_Shutdown();

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

	Sys_Shutdown();

	exit(1);
}

void Sys_Printf(char *fmt, ...)
{
	va_list va;
	char text[2048];
	char *p;

	if (!dedicated)
		return;

	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);

	p = text;

	while(*p != 0)
	{
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);

		p++;
	}
}

char *Sys_ConsoleInput()
{
	return 0;
}

double Sys_DoubleTime()
{
	struct timeval tp;
	static int secbase;

	gettimeofday(&tp, 0);

	if (secbase == 0)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

char *Sys_GetClipboardData()
{
	return 0;
}

void Sys_CopyToClipboard(char *text)
{
}

void Sys_mkdir(char *path)
{
	BPTR lock;

	lock = CreateDir(path);
	if (lock)
	{
		UnLock(lock);
	}
}

int main(int argc, char **argv)
{
	double time, oldtime, newtime;

	signal(SIGINT, SIG_IGN);

	SocketBase = OpenLibrary("bsdsocket.library", 0);

	COM_InitArgv(argc, argv);

	Host_Init(argc, argv, 16*1024*1024);

	oldtime = Sys_DoubleTime();
	while((SetSignal(0, 0)&SIGBREAKF_CTRL_C) == 0)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);
	}

	Sys_Error("End of app");

	return 0;
}
