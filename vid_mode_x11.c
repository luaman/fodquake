#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"
#include "vid_mode_x11.h"

int modeline_to_x11mode(const char *modeline, struct x11mode *x11mode)
{
	const char *p;
	unsigned int commas;

	commas = 0;
	p = modeline;
	while((p = strchr(p, ',')))
	{
		commas++;
		p = p + 1;
	}

	if (commas != 10)
		return 0;

	p = modeline;
	x11mode->dotclock = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->hdisplay = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->hsyncstart = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->hsyncend = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->htotal = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->hskew = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->vdisplay = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->vsyncstart = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->vsyncend = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->vtotal = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	x11mode->flags = strtoul(p, 0, 0);

	return 1;
}

const char * const *Sys_Video_GetModeList(void)
{
	Display *disp;
	int scrnum;
	int version;
	int revision;
	int num_vidmodes;
	XF86VidModeModeInfo **vidmodes;
	const char **ret;
	char buf[256];
	unsigned int i;

	ret = 0;

	disp = XOpenDisplay(NULL);
	if (disp)
	{
		scrnum = DefaultScreen(disp);

		if (XF86VidModeQueryVersion(disp, &version, &revision))
		{
			if (XF86VidModeGetAllModeLines(disp, scrnum, &num_vidmodes, &vidmodes))
			{
				i = 0;

				if (num_vidmodes < 65536)
				{
					ret = malloc(sizeof(*ret) * (num_vidmodes + 1));
					if (ret)
					{
						for(i=0;i<num_vidmodes;i++)
						{
							snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", vidmodes[i]->dotclock, vidmodes[i]->hdisplay, vidmodes[i]->hsyncstart, vidmodes[i]->hsyncend, vidmodes[i]->htotal, vidmodes[i]->hskew, vidmodes[i]->vdisplay, vidmodes[i]->vsyncstart, vidmodes[i]->vsyncend, vidmodes[i]->vtotal, vidmodes[i]->flags);

							ret[i] = malloc(strlen(buf)+1);
							if (ret[i] == 0)
								break;

							strcpy((void *)ret[i], buf);
						}

						ret[i] = 0;

						if (i != num_vidmodes)
						{
							Sys_Video_FreeModeList(ret);
							ret = 0;
						}

					}
				}

				XFree(vidmodes);
			}
		}

		XCloseDisplay(disp);
	}

	return ret;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;

	for(i=0;displaymodes[i];i++)
	{
		free((void *)displaymodes[i]);
	}

	free((void *)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	char buf[256];
	char *ret;
	struct x11mode x11mode;
	unsigned long long dotclock;

	if (modeline_to_x11mode(mode, &x11mode))
	{
		dotclock = x11mode.dotclock;

		snprintf(buf, sizeof(buf), "%dx%d, %dHz", x11mode.hdisplay, x11mode.vdisplay, (int)((((dotclock*1000000)/(x11mode.htotal*x11mode.vtotal))+500)/1000));

		ret = malloc(strlen(buf) + 1);
		if (ret)
		{
			strcpy(ret, buf);

			return ret;
		}
	}

	return 0;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void *)modedescription);
}

