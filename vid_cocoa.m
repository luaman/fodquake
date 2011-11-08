#import <AppKit/AppKit.h>

#undef true
#undef false

#import "quakedef.h"
#import "input.h"
#import "keys.h"
#import "gl_local.h"
#import "in_macosx.h"

struct display
{
	NSAutoreleasePool *pool;
	qboolean fullscreen;
	struct input_data *input;
	NSWindow *window;
	unsigned int width;
	unsigned int height;
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
- (void)keyDown:(NSEvent*)event
{
}
@end

#ifdef GLQUAKE
static qboolean vid_vsync_callback(cvar_t *var, char *value)
{
	var->value = Q_atof(value);
	GLint swapInterval = var->value;
	
	if (NSApp)
	{
		[[[[NSApp keyWindow] contentView] openGLContext] setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
	}
	
	return false;
}

cvar_t vid_vsync = {"vid_vsync", "1", 0, vid_vsync_callback};
#endif

void Sys_Video_CvarInit(void)
{
#ifdef GLQUAKE
	Cvar_Register(&vid_vsync);
#endif
}

void* Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;
	
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
					d->window = [[NSMyWindow alloc] initWithContentRect:[[NSScreen mainScreen] frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
				}
				else
				{
					d->window = [[NSMyWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height) styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask backing:NSBackingStoreBuffered defer:YES];
				}
				
				if (d->window)
				{
					if (fullscreen)
					{
						[d->window setLevel:NSMainMenuWindowLevel + 1];
					}
					else
					{
						[d->window center];
					}
					
					d->fullscreen = fullscreen ? true : false;
					d->input = Sys_Input_Init();
					if (d->input)
					{
#ifdef GLQUAKE
						NSOpenGLPixelFormat *pixelFormat;
						NSOpenGLView *openglview;
						GLint swapInterval = vid_vsync.value;
						
						NSOpenGLPixelFormatAttribute attributes[] =
						{
							NSOpenGLPFADoubleBuffer,
							NSOpenGLPFAAccelerated,
							NSOpenGLPFAColorSize, 24,
							NSOpenGLPFADepthSize, 16,
							0
						};
						
						pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
						if (pixelFormat)
						{
							NSRect rect;

							if (fullscreen)
							{
								rect = [d->window frame];
							}
							else
							{
								rect.origin.x = 0;
								rect.origin.y = [d->window frame].size.height - height;
								rect.size.width = width;
								rect.size.height = height;
							}

							d->width = rect.size.width;
							d->height = rect.size.height;

							openglview = [[NSOpenGLView alloc] initWithFrame:rect pixelFormat:pixelFormat];
							[pixelFormat release];
							if (openglview)
							{
								[d->window setContentView:openglview];
								[[openglview openGLContext] setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

								[d->window useOptimizedDrawing:YES];
								[d->window makeKeyAndOrderFront:nil];
								
								return d;
							}
						}
#else
						NSBitmapImageRep *bitmapRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:1 hasAlpha:NO isPlanar:NO colorSpaceName:NSCalibratedWhiteColorSpace bytesPerRow:0 bitsPerPixel:8];
						if (bitmapRep)
						{
							NSGraphicsContext *context = [[NSGraphicsContext alloc] graphicsContextWithBitmapImageRep:bitmapRep];
							if (context)
							{
								return d;
							}
							
							[bitmapRep release];
						}
#endif
						Sys_Input_Shutdown(d->input);

					}
					
					[d->window close];
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
	
	[[d->window contentView] release];
	[d->window close];
	
	[d->pool release];
	
	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 1;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = (struct display*)display;

	[[[d->window contentView] openGLContext] flushBuffer];
}

int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down)
{
	struct display *d = (struct display*)display;
	
	if ([d->window isKeyWindow] == FALSE)
	{
		while (Sys_Input_GetKeyEvent(d->input, keynum, down))
		{
		}
		
		return 0;
	}
	
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
	struct display *d = (struct display*)display;

	[d->window setTitle:[NSString stringWithCString:text encoding:[NSString defaultCStringEncoding]]];
}

unsigned int Sys_Video_GetWidth(void *display)
{
	struct display *d = (struct display*)display;

	return d->width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	struct display *d = (struct display*)display;

	return d->height;
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
	struct display *d = (struct display*)display;
	NSEvent *event;
	
	*x = 0;
	*y = 0;
	*width = d->width;
	*height = d->height;
	
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

void *qglGetProcAddress(const char *p)
{
	void *ret;
	
	ret = 0;
	
	if (strcmp(p, "glMultiTexCoord2fARB") == 0)
		ret = glMultiTexCoord2f;
	else if (strcmp(p, "glActiveTextureARB") == 0)
		ret = glActiveTexture;
	else if (strcmp(p, "glBindBufferARB") == 0)
		ret = glBindBuffer;
	else if (strcmp(p, "glBufferDataARB") == 0)
		ret = glBufferData;
	else
		Sys_Error("Unknown OpenGL function \"%s\"\n", p);
	
	return ret;
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

void VID_LockBuffer()
{
}

void VID_UnlockBuffer()
{
}
#endif
