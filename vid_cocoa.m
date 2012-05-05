/*
 Copyright (C) 2011 Florian Zwoch
 Copyright (C) 2011 Mark Olsen
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#import <AppKit/AppKit.h>

#undef true
#undef false

#import "quakedef.h"
#import "input.h"
#import "keys.h"
#import "gl_local.h"
#import "in_macosx.h"

extern cvar_t in_grab_windowed_mouse;

struct display
{
	qboolean fullscreen;
	struct input_data *input;
	NSWindow *window;
	unsigned int width;
	unsigned int height;
	
#ifndef GLQUAKE
	unsigned char *rgb_buf;
	unsigned char *buf;
	unsigned int rowbytes;
	unsigned char palette[256][3];
#endif
};

@interface NSMyWindow : NSWindow
{
	struct display *d;
}
- (void)setDisplayStructPointer:(struct display*)display;
@end

@implementation NSMyWindow
- (void)setDisplayStructPointer:(struct display*)display
{
	d = display;
}
- (BOOL)canBecomeKeyWindow
{
	return YES;
}
- (void)keyDown:(NSEvent*)event
{
	if ([event keyCode] == 4 && [event modifierFlags] & NSCommandKeyMask)
	{
		[NSApp hide:nil];
	}
}
- (void)applicationDidBecomeActive:(NSNotification*)notification
{
	if (d->fullscreen)
	{
		[self setLevel:NSMainMenuWindowLevel + 1];
	}
}
- (void)applicationDidResignActive:(NSNotification*)notification
{
	if (d->fullscreen)
	{
		[self setLevel:NSNormalWindowLevel - 1];
	}
}
- (void)applicationDidUnhide:(NSNotification*)notification
{
	CGPoint point = { -1.0, -1.0 };
	
	if (d->fullscreen)
	{
		CGWarpMouseCursorPosition(point);
	}
	else if (in_grab_windowed_mouse.value == 1)
	{
		CGWarpMouseCursorPosition(point);
	}
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

int Sys_Video_Init()
{
	return 1;
}

void Sys_Video_Shutdown()
{
}

void* Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;
	
	d = malloc(sizeof(struct display));
	if (d)
	{
		memset(d, 0, sizeof(struct display));
		
		if (mode)
		{
			unsigned int flags;
			CFArrayRef modes;
			
			sscanf(mode, "%u,%u,%u", &width, &height, &flags);
			
			modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
			if (modes)
			{
				unsigned int num_modes = CFArrayGetCount(modes);
				unsigned int i;
				
				for (i = 0; i < num_modes; i++)
				{
					CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
					
					if (width == CGDisplayModeGetWidth(mode) && height == CGDisplayModeGetHeight(mode) && flags == CGDisplayModeGetIOFlags(mode))
					{
						CGDisplayConfigRef ConfigRef;
						CGError err;
						
						// hmz.. capturuing shall be released at soem point..
						// this makes it difficult across all those different modes..
						err = CGDisplayCapture(CGMainDisplayID());
						if (err == kCGErrorSuccess)
						{
							err = CGBeginDisplayConfiguration(&ConfigRef);
							if (err == kCGErrorSuccess)
							{
								err = CGConfigureDisplayWithDisplayMode(ConfigRef, CGMainDisplayID(), mode, NULL);
								if (err == kCGErrorSuccess)
								{
									err = CGCompleteDisplayConfiguration(ConfigRef, kCGConfigureForAppOnly);
								}
							}
						}
						
						break;
					}
				}
				
				CFRelease(modes);
			}
		}
		
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
			NSRect rect;
			
			[((NSMyWindow*)d->window) setDisplayStructPointer:d];
			
			if (fullscreen)
			{
				rect = [d->window frame];
				
				d->fullscreen = true;
				
				if (mode)
				{
					[d->window setLevel:CGShieldingWindowLevel()];
				}
				else
				{
					[d->window setLevel:NSMainMenuWindowLevel + 1];
				}
			}
			else
			{
				rect.origin.x = 0;
				rect.origin.y = [d->window frame].size.height - height;
				rect.size.width = width;
				rect.size.height = height;
				
				d->fullscreen = false;
				
				[d->window center];
			}
			
			d->width = rect.size.width;
			d->height = rect.size.height;
			
			d->input = Sys_Input_Init();
			if (d->input)
			{
				Sys_Input_SetFnKeyBehavior(d->input, [[[[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain] objectForKey:@"com.apple.keyboard.fnState"] intValue]);
				
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
					openglview = [[NSOpenGLView alloc] initWithFrame:rect pixelFormat:pixelFormat];
					[pixelFormat release];
					if (openglview)
					{
						[d->window setContentView:openglview];
						[[openglview openGLContext] setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

						[d->window useOptimizedDrawing:YES];
						[d->window makeKeyAndOrderFront:nil];
						[NSApp setDelegate:d->window];
						
						return d;
					}
				}
#else
				d->buf = (unsigned char*)malloc(d->width * d->height);
				if (d->buf)
				{
					NSBitmapImageRep *img = [[NSBitmapImageRep alloc]
						initWithBitmapDataPlanes:&d->buf
						pixelsWide:d->width
						pixelsHigh:d->height
						bitsPerSample:8
						samplesPerPixel:3
						hasAlpha:NO
						isPlanar:NO
						colorSpaceName:@"NSDeviceRGBColorSpace"
						bytesPerRow:0
						bitsPerPixel:0];
					
					if (img)
					{
						d->rowbytes = [img bytesPerRow];
						
						[img release];
					}
					else
					{
						d->rowbytes = d->width * 3;
					}
					
					d->rgb_buf = (unsigned char*)malloc(d->rowbytes * d->height);
					if (d->rgb_buf)				
					{
						[d->window useOptimizedDrawing:YES];
						[d->window makeKeyAndOrderFront:nil];
						[NSApp setDelegate:d->window];
						
						return d;
					}
					
					free(d->buf);
				}
#endif
				Sys_Input_Shutdown(d->input);

			}
			
			[d->window close];
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
	
#ifndef GLQUAKE
	free(d->rgb_buf);
	free(d->buf);
#endif
	
	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 1;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = (struct display*)display;
	NSEvent *event;

#ifdef GLQUAKE
	[[[d->window contentView] openGLContext] flushBuffer];
#else	
	int i, j;
	unsigned char *src = d->buf;	
	unsigned char *dst = d->rgb_buf;
	NSBitmapImageRep *img;
	
	for (i = 0; i < d->height; i++)
	{
		for (j = 0; j < d->width; j++)
		{
			dst[0] = d->palette[src[0]][0];
			dst[1] = d->palette[src[0]][1];
			dst[2] = d->palette[src[0]][2];
			
			src++;
			dst += 3;
		}
		
		dst += d->rowbytes - d->width * 3;
	}

	[[d->window contentView] lockFocus];	
	
	img = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:&d->rgb_buf
		pixelsWide:d->width
		pixelsHigh:d->height
		bitsPerSample:8
		samplesPerPixel:3
		hasAlpha:NO
		isPlanar:NO
		colorSpaceName:@"NSDeviceRGBColorSpace"
		bytesPerRow:0
		bitsPerPixel:0];
	
	[img draw];
	[img release];
	
	[[d->window contentView] unlockFocus];
	
	[d->window flushWindow];
#endif
	
	while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSEventTrackingRunLoopMode dequeue:YES]))
	{
		[NSApp sendEvent:event];
		[NSApp updateWindows];
	}
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
	struct display *d = (struct display*)display;
	char buf[64];
	const char *ret;
	
	snprintf(buf, sizeof(buf), "%u,%u,%u", d->width, d->height, 0);
	
	ret = strdup(buf);
	if (ret == NULL)
	{
		return NULL;
	}
	
	return ret;
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

#ifdef GLQUAKE
void Sys_Video_BeginFrame(void *display, unsigned int *x, unsigned int *y, unsigned int *width, unsigned int *height)
{
	struct display *d = (struct display*)display;
	
	*x = 0;
	*y = 0;
	*width = d->width;
	*height = d->height;
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
	struct display *d = (struct display*)display;
	
	memcpy(d->palette, palette, sizeof(d->palette));
}

unsigned int Sys_Video_GetBytesPerRow(void *display)
{
	struct display *d = (struct display*)display;
	
	return d->width;
}

void *Sys_Video_GetBuffer(void *display)
{
	struct display *d = (struct display*)display;
	
	return d->buf;
}

void VID_LockBuffer()
{
}

void VID_UnlockBuffer()
{
}
#endif
