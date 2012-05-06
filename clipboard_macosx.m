#import <AppKit/AppKit.h>

const char *Sys_Video_GetClipboardText(void *display)
{
	NSString *string = [[NSPasteboard generalPasteboard] stringForType:NSStringPboardType];
	char *text;	
	
	text = (char*)malloc(strlen([string cStringUsingEncoding:NSASCIIStringEncoding]) + 1);
	if (text == NULL)
	{
		return NULL;
	}
	
	sprintf(text, "%s", [string cStringUsingEncoding:NSASCIIStringEncoding]);
	
	return text;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	free((char*)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
	NSString *string = [[NSString alloc] initWithCString:text encoding:NSASCIIStringEncoding];
	
	[[NSPasteboard generalPasteboard] setString:string forType:NSStringPboardType];
	[string release];
	
	return;
}

