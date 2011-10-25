#import <AppKit/AppKit.h>
#import <mach/mach_time.h>

#import <stdio.h>
#import <stdlib.h>
#import <stdarg.h>

#undef true
#undef false

#import "common.h"

static mach_timebase_info_data_t tbinfo;
static int randomfd;

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

void Sys_MicroSleep(unsigned int microseconds)
{
        usleep(microseconds);
}

void Sys_RandomBytes(void *target, unsigned int numbytes)
{
        ssize_t s;

        while(numbytes)
        {
                s = read(randomfd, target, numbytes);
                if (s < 0)
                {
                        Sys_Error("Linux sucks");
                }

                numbytes -= s;
                target += s;
        }
}

unsigned long long Sys_IntTime()
{
	return monotonictime() / 1000;
}

const char *Sys_GetRODataPath(void)
{
	char *ret = NULL;
	
	ret = malloc([[[NSBundle mainBundle] resourcePath] lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1);
	if (ret)
	{
		sprintf(ret, "%s", [[[NSBundle mainBundle] resourcePath] UTF8String]);
	}
	
	return ret;
}

const char *Sys_GetUserDataPath(void)
{
	char *ret = NULL;
	NSString *home = NSHomeDirectory();
	
	if (home)
	{
		ret = malloc([home lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + strlen("/Library/Application Support/FodQuake") + 1);
		if (ret)
		{
			sprintf(ret, "%s/Library/Application Support/FodQuake", [home UTF8String]);
		}
	}
	
	return ret;
}

const char *Sys_GetLegacyDataPath(void)
{
	return NULL;
}

void Sys_FreePathString(const char *x)
{
	free((void*)x);
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
	
	if (NSApp == nil)
	{
		[NSApplication sharedApplication];
	}
	if (NSApp)
	{
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		if (pool)
		{
			NSString *user = [[NSString alloc] initWithCString:string encoding:NSASCIIStringEncoding];
			if (user)
			{
				NSAlert *alert = [[NSAlert alloc] init];
				if (alert)
				{
					[alert setMessageText:@"FodQuake error"];
					[alert setInformativeText:user];
					[alert runModal];
					[alert release];
				}
				
				[user release];
			}
			
			[pool release];
		}
		
		[NSApp release];
	}

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

	randomfd = open("/dev/urandom", O_RDONLY);
        if (randomfd == -1)
                Sys_Error("Unable to open /dev/urandom");

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

