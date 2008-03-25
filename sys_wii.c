#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#undef TRUE
#undef FALSE
#undef BOOL
#define BOOL FBOOL

#include "fatfs/ff.h"

#undef BOOL

#define true qtrue
#define false qfalse
#include "quakedef.h"
#include "server.h"
#undef false
#undef true

void Sys_Printf (char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

void Sys_Error (char *error, ...)
{
	va_list va;

	printf("Sys_Error: %s\n", error);

	va_start(va, error);
	vprintf(error, va);
	va_end(va);

	while(1);
}

void Sys_Quit()
{
	while(1);
}

void Sys_mkdir(char *path)
{
}

char *Sys_ConsoleInput()
{
	return 0;
}

const char *Sys_Video_GetClipboardText(void *display)
{
	return 0;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
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

void Sys_CvarInit()
{
}

void Sys_Init()
{
}

static int main_real()
{
	printf("Mounting drive\n");

	{
		FATFS fso;
		FIL myfile;
		int r;

		memset(&fso, 0, sizeof(fso));
		memset(&myfile, 0, sizeof(myfile));

		r = f_mount(0, &fso);

		if (r == 0)
			printf("Succeeded\n");
		else
			printf("Failed\n");
	}

	{
	double time, oldtime, newtime;
	char *myargv[] = { "fodquake", 0 };

	printf("Calling Host_Init()\n");

#if 0
	cl.frames = malloc(sizeof(*cl.frames)*UPDATE_BACKUP);
	memset(cl.frames, 0, sizeof(*cl.frames)*UPDATE_BACKUP);
#endif
	Host_Init(1, myargv, 10*1024*1024);

	oldtime = Sys_DoubleTime();
	while(1)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		time = 0.015;
		Host_Frame(time);
	}

	Sys_Error("End of app");

	return 0;
	}
}

int main()
{
	void *xfb;
	GXRModeObj *rmode;
	lwp_t handle;
	int r;
	char *stack;

	IOS_ReloadIOS(30);

	VIDEO_Init();

	switch(VIDEO_GetCurrentTvMode())
	{
		case VI_NTSC:
			rmode = &TVNtsc480IntDf;
			break;

		case VI_PAL:
			rmode = &TVPal528IntDf;
			break;

		case VI_MPAL:
			rmode = &TVMpal480IntDf;
			break;

		default:
			rmode = &TVNtsc480IntDf;
			break;
	}

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

#if 1
	printf("Calling main()\n");
	main_real();
	printf("main() returned\n");
#else
	printf("Creating main thread\n");

	stack = malloc(1024*1024);
	if (stack == 0)
	{
		printf("Unable to allocate stack\n");
		while(1);
	}

	handle = 0;
	r = LWP_CreateThread(&handle, main_real, 0, stack, 1024*1024, 50);
	if (r != 0)
	{
		printf("Failed to create thread\n");
		while(1);
	}
	printf("Main thread created\n");
	LWP_SetThreadPriority(0, 0);

	printf("Looping\n");
	while(1);
#endif
}

int getuid()
{
	return 42;
}

