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

#include <stdlib.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>

const char * const *Sys_Video_GetModeList(void)
{
	char buf[64];
	const char **ret;
	unsigned int width;
	unsigned int height;
	unsigned int num_modes;
	unsigned int i;
	
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), NULL);
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
		CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		
		width = CGDisplayModeGetWidth(mode);
		height = CGDisplayModeGetHeight(mode);
		
		snprintf(buf, sizeof(buf), "%u,%u", width, height);
		
		ret[i] = strdup(buf);
		if (ret[i] == NULL)
		{
			unsigned int j;
			
			for (j = 0; j < i; j++)
			{
				free(ret[j]);
			}
			
			CFRelease(modes);
			
			return NULL;
		}
	}
	
	CFRelease(modes);
	
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
	
	if (sscanf(mode, "%u,%u", &width, &height) != 2)
	{
		return NULL;
	}
	
	snprintf(buf, sizeof(buf), "%ux%u", width, height);
	
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

