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

#include <stdlib.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>

#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED && __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_10_6
#define __USE_DEPRECATED_APIS
#endif

#ifdef __USE_DEPRECATED_APIS
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
#endif

const char * const *Sys_Video_GetModeList(void)
{
	char buf[64];
	const char **ret;
	unsigned int num_modes;
	unsigned int i;
	
#ifndef __USE_DEPRECATED_APIS
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
#else
	CFArrayRef modes = CGDisplayAvailableModes(CGMainDisplayID());
#endif
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
		
#ifndef __USE_DEPRECATED_APIS
		CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		
		width = CGDisplayModeGetWidth(mode);
		height = CGDisplayModeGetHeight(mode);
		flags = CGDisplayModeGetIOFlags(mode);
#else
		CFDictionaryRef mode = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
		
		width = GetDictionaryLong(mode, kCGDisplayWidth);
		height = GetDictionaryLong(mode, kCGDisplayHeight);
		flags = GetDictionaryLong(mode, kCGDisplayIOFlags);
#endif
		snprintf(buf, sizeof(buf), "%u,%u,%u", width, height, flags);
		
		ret[i] = strdup(buf);
		if (ret[i] == NULL)
		{
			unsigned int j;
			
			for (j = 0; j < i; j++)
			{
				free((char*)ret[j]);
			}
#ifndef __USE_DEPRECATED_APIS
			CFRelease(modes);
#endif
			return NULL;
		}
	}
	
#ifndef __USE_DEPRECATED_APIS
	CFRelease(modes);
#endif
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
	const char *ret;
	unsigned int width;
	unsigned int height;
	unsigned int flags;
	
	if (sscanf(mode, "%u,%u,%u", &width, &height, &flags) != 3)
	{
		return NULL;
	}
	
	snprintf(buf, sizeof(buf), "%ux%u%s", width, height, (flags & kDisplayModeStretchedFlag) ? " (stretched)" : "");
	
	ret = strdup(buf);
	if (ret == NULL)
	{
		return NULL;
	}
	
	return ret;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void*)modedescription);
}
