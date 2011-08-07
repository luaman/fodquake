#import <AppKit/AppKit.h>

#undef true
#undef false

#import "quakedef.h"
#import "input.h"
#import "keys.h"
#import "gl_local.h"
#import "in_macosx.h"

struct display {
	NSAutoreleasePool *pool;
	qboolean fullscreen;
	struct input_data *input;
};

@interface NSMyWindow : NSWindow
{
}
@end

@implementation NSMyWindow
- (BOOL)canBecomeKeyWindow
{
	return YES;
}
@end

@interface NSMyOpenGLView : NSOpenGLView
{
}
@end

@implementation NSMyOpenGLView 
- (BOOL)acceptsFirstResponder
{
	return YES;
}
- (void)keyDown:(NSEvent*)event
{
}
@end

void Sys_Video_CvarInit(void)
{
}

void* Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;
	NSWindow *window;
	NSOpenGLPixelFormat *pixelformat;
	NSOpenGLView *openglview;
	GLint swapInterval = 1;
	
	NSOpenGLPixelFormatAttribute attributes[] =
	{
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFADepthSize, 16,
		0
	};
	
	d = malloc(sizeof(struct display));
	if (d)
	{
		memset(d, 0, sizeof(struct display));
		
		if (NSApp == nil)
		{
			[NSApplication sharedApplication];
		}
		if (NSApp)
		{
			d->pool = [[NSAutoreleasePool alloc] init];
			if (d->pool)
			{
				if (fullscreen)
				{
					window = [[NSMyWindow alloc] initWithContentRect:[[NSScreen mainScreen] frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
				}
				else
				{
					window = [[NSMyWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height) styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask backing:NSBackingStoreBuffered defer:YES];
				}
				
				if (window)
				{
					if (fullscreen)
					{
						[window setLevel:NSMainMenuWindowLevel + 1];
					}
					else
					{
						[window center];
					}
					
					pixelformat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
					if (pixelformat)
					{
						openglview = [[NSMyOpenGLView alloc] initWithFrame:[window frame] pixelFormat:pixelformat];
						if (openglview)
						{
							[pixelformat release];
							[window setContentView:openglview];
							[[openglview openGLContext] setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
							
							[window useOptimizedDrawing:YES];
							[window makeKeyAndOrderFront:nil];
							
							d->fullscreen = fullscreen ? true : false;
							d->input = Sys_Input_Init();
							if (d->input)
							{
								return d;
							}
							
							[openglview release];
						}
						
						[pixelformat release];
					}
					
					[window close];
				}
				
				[d->pool release];
			}
			
			[NSApp release];
		}
		
		free(d);
	}
	
	return NULL;
}

void Sys_Video_Close(void *display)
{
	struct display *d = (struct display*)display;
	
	Sys_Input_Shutdown(d->input);
	
	[[[NSApp keyWindow] contentView] release];
	[[NSApp keyWindow] close];
	
	[d->pool release];
	
	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 1;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	[[[[NSApp keyWindow] contentView] openGLContext] flushBuffer];
}

int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down)
{
	struct display *d = (struct display*)display;
	
	return Sys_Input_GetKeyEvent(d->input, keynum, down);
}

void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = (struct display*)display;

	Sys_Input_GetMouseMovement(d->input, mousex, mousey);
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = (struct display*)display;
	
	if (dograb)
	{
		[NSCursor hide];
		CGAssociateMouseAndMouseCursorPosition(NO);
	}
	else
	{
		[NSCursor unhide];
		CGAssociateMouseAndMouseCursorPosition(YES);
	}
	
	Sys_Input_GrabMouse(d->input, dograb);
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	[[NSApp keyWindow] setTitle:[NSString stringWithCString:text encoding:[NSString defaultCStringEncoding]]];
}

unsigned int Sys_Video_GetWidth(void *display)
{
	return [[NSApp keyWindow] frame].size.width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	return [[NSApp keyWindow] frame].size.height;
}

qboolean Sys_Video_GetFullscreen(void *display)
{
	struct display *d = (struct display*)display;
	
	return d->fullscreen;
}

const char* Sys_Video_GetMode(void *display)
{
	return NULL;
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

#ifdef GLQUAKE
void Sys_Video_BeginFrame(void *display, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height)
{
	NSEvent *event;
	
	*x = 0;
	*y = 0;
	*width = [[NSApp keyWindow] frame].size.width;
	*height = [[NSApp keyWindow] frame].size.height;
	
	while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSEventTrackingRunLoopMode dequeue:YES]))
	{
		[NSApp sendEvent:event];
		[NSApp updateWindows];
	}
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
	int i;
	float table[256 * 3];
	
	for (i = 0; i < 256 * 3; i++)
	{
		table[i] = (float)ramps[i] / 0xffff;
	}
	
	CGSetDisplayTransferByTable(0, 256, table, table + 256, table + 512);
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	return true;
}
#else
void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
}

unsigned int Sys_Video_GetBytesPerRow(void *display)
{
	return 0;
}

void *Sys_Video_GetBuffer(void *display)
{
	return NULL;
}

VID_LockBuffer()
{
}

VID_UnlockBuffer()
{
}
#endif
