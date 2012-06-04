/*
 Copyright (C) 2011-2012 Florian Zwoch
 Copyright (C) 2011-2012 Mark Olsen
 
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

#include <AppKit/AppKit.h>

#ifndef NSAppKitVersionNumber10_6
#define NSAppKitVersionNumber10_6 1038
typedef struct _CGDisplayConfigRef * CGDisplayConfigRef;
#endif

extern CGDisplayModeRef (*_CGDisplayCopyDisplayMode)(CGDirectDisplayID display);
extern CGError (*_CGBeginDisplayConfiguration)(CGDisplayConfigRef *pConfigRef);
extern CGError (*_CGConfigureDisplayWithDisplayMode)(CGDisplayConfigRef config, CGDirectDisplayID display, CGDisplayModeRef mode, CFDictionaryRef options);
extern CGError (*_CGCompleteDisplayConfiguration)(CGDisplayConfigRef configRef, CGConfigureOption option);
extern CGError (*_CGCancelDisplayConfiguration)(CGDisplayConfigRef configRef);
extern CFArrayRef (*_CGDisplayCopyAllDisplayModes)(CGDirectDisplayID display, CFDictionaryRef options);
extern size_t (*_CGDisplayModeGetWidth)(CGDisplayModeRef mode);
extern size_t (*_CGDisplayModeGetHeight)(CGDisplayModeRef mode);
extern uint32_t (*_CGDisplayModeGetIOFlags)(CGDisplayModeRef mode);

static long GetDictionaryLong(CFDictionaryRef theDict, const void *key)
{
	long value = 0;
	CFNumberRef numRef;
	
	numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
	if (numRef != NULL)
	{
		CFNumberGetValue(numRef, kCFNumberLongType, &value);
	}
	
	return value;
}

const char * const *Sys_Video_GetModeList(void)
{
	char buf[64];
	const char **ret;
	unsigned int num_modes;
	unsigned int i;
	CFArrayRef modes;
	
	if (NSAppKitVersionNumber < NSAppKitVersionNumber10_6)
	{
		modes = CGDisplayAvailableModes(CGMainDisplayID());
	}
	else
	{
		modes = _CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
	}

	if (modes == NULL)
	{
		return NULL;
	}
	
	num_modes = CFArrayGetCount(modes);
	
	ret = malloc((num_modes + 1) * sizeof(*ret));
	if (ret == NULL)
	{
		return NULL;
	}
	
	for (i = 0; i < num_modes; i++)
	{
		unsigned int width;
		unsigned int height;
		unsigned int flags;
		CFDictionaryRef mode_legacy;
		CGDisplayModeRef mode;
		
		if (NSAppKitVersionNumber < NSAppKitVersionNumber10_6)
		{
			mode_legacy = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
			
			width = GetDictionaryLong(mode_legacy, kCGDisplayWidth);
			height = GetDictionaryLong(mode_legacy, kCGDisplayHeight);
			flags = GetDictionaryLong(mode_legacy, kCGDisplayIOFlags);
		}
		else
		{
			mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
			
			width = _CGDisplayModeGetWidth(mode);
			height = _CGDisplayModeGetHeight(mode);
			flags = _CGDisplayModeGetIOFlags(mode);
		}

		snprintf(buf, sizeof(buf), "%u,%u,%u", width, height, flags);
		
		ret[i] = strdup(buf);
		if (ret[i] == NULL)
		{
			unsigned int j;
			
			for (j = 0; j < i; j++)
			{
				free((char*)ret[j]);
			}
			
			free(ret);

			if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_6)
			{
				CFRelease(modes);
			}
			
			return NULL;
		}
	}
	
	if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_6)
	{
		CFRelease(modes);
	}
	
	ret[num_modes] = NULL;
	
	return ret;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;
	
	for (i = 0; displaymodes[i] != NULL; i++)
	{
		free((void*)displaymodes[i]);
	}
	
	free((void*)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	char buf[64];
	unsigned int width;
	unsigned int height;
	unsigned int flags;
	
	if (sscanf(mode, "%u,%u,%u", &width, &height, &flags) != 3)
	{
		return NULL;
	}
	
	snprintf(buf, sizeof(buf), "%ux%u%s", width, height, (flags & kDisplayModeStretchedFlag) ? " (stretched)" : "");
	
	return strdup(buf);
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void*)modedescription);
}
