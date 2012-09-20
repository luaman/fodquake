/*
Copyright (C) 2012 Mark Olsen

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

#include <string.h>
#include <stdlib.h>

#include "cl_view.h"
#include "skinimp.h"
#include "d_skinimp.h"

struct SkinImp *SkinImp_CreateSolidColour(float *colours)
{
	struct SkinImp *skinimp;

	skinimp = malloc(sizeof(*skinimp));
	if (skinimp)
	{
		skinimp->data = 0;
		skinimp->width = 0;
		skinimp->height = 0;
		skinimp->colour = V_LookUpColourNoFullbright(colours[0], colours[1], colours[2]);

		return skinimp;
	}

	return 0;
}

struct SkinImp *SkinImp_CreateTexturePaletted(void *data, unsigned int width, unsigned int height, unsigned int modulo)
{
	struct SkinImp *skinimp;
	unsigned char *src;
	unsigned char *dst;
	unsigned int x;
	unsigned int y;

	if (width > 296)
		width = 296;
	if (height > 194)
		height = 194;

	skinimp = malloc(sizeof(*skinimp));
	if (skinimp)
	{
		skinimp->data = malloc(296*194);
		if (skinimp->data)
		{
			skinimp->width = 296;
			skinimp->height = 194;

			src = data;
			dst = skinimp->data;

			for(y=0;y<height;y++)
			{
				for(x=0;x<width;x++)
				{
					dst[x] = src[x];
				}

				for(;x<296;x++)
					dst[x] = 0;

				src += modulo;
				dst += 296;
			}

			if (height != 194)
				memset(dst, 0, (194-height)*296);

			return skinimp;
		}

		free(skinimp);
	}

	return 0;
}

void SkinImp_Destroy(struct SkinImp *skinimp)
{
	free(skinimp->data);
	free(skinimp);
}

