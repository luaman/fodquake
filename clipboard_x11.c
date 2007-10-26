#include <string.h>

const char *Sys_Video_GetClipboardText(void *display)
{
	struct display *d;
	char *xbuf;
	char *buf;
	int len;

	d = display;
	buf = 0;

	xbuf = XFetchBytes(d->x_disp, &len);
	if (xbuf)
	{
		buf = malloc(len+1);
		if (buf)
		{
			memcpy(buf, xbuf, len);
			buf[len] = 0;
		}

		XFree(xbuf);
	}

	return buf;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	free((char *)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
	struct display *d;

	d = display;

	XStoreBytes(d->x_disp, text, strlen(text));
}

