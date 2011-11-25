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
static NSAutoreleasePool *pool;

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
                        Sys_Error("Could not read from randomfd");
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
	
	ret = malloc([[[NSBundle mainBundle] resourcePath] lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + strlen("/data") + 1);
	if (ret)
	{
		sprintf(ret, "%s/data", [[[NSBundle mainBundle] resourcePath] UTF8String]);
	}
	
	return ret;
}

const char *Sys_GetUserDataPath(void)
{
	char *ret = NULL;
	NSString *home = NSHomeDirectory();
	
	if (home)
	{
		ret = malloc([home lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + strlen("/Library/Application Support/Fodquake") + 1);
		if (ret)
		{
			sprintf(ret, "%s/Library/Application Support/Fodquake", [home UTF8String]);
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
	
	if (NSApp)
	{
		if (pool)
		{
			NSString *user = [[NSString alloc] initWithCString:string encoding:NSASCIIStringEncoding];
			if (user)
			{
				NSAlert *alert = [[NSAlert alloc] init];
				if (alert)
				{
					[alert setMessageText:@"Fodquake error"];
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
	
	[NSApplication sharedApplication];
	if (NSApp == nil)
		Sys_Error("Error init shared application");
	
	pool = [[NSAutoreleasePool alloc] init];
	if (pool == nil)
		Sys_Error("Error creating auto release pool");

	mach_timebase_info(&tbinfo);

	COM_InitArgv(argc, argv);

/* Because Apple's Mac OS X 10.6 headers don't have this... */
#ifndef NSAppKitVersionNumber10_5
#define NSAppKitVersionNumber10_5 949
#endif

	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_5)
		Sys_Error("Fodquake requires Mac OS X 10.5 or higher");
	
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

