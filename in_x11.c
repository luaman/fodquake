/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2006-2007 Mark Olsen

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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/xf86dga.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#include "in_x11.h"

#define XINPUTFLAGS (StructureNotifyMask|KeyPressMask|KeyReleaseMask|ExposureMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask|FocusChangeMask)

cvar_t cl_keypad = { "cl_keypad", "1" };
cvar_t in_dga_mouse = { "in_dga_mouse", "0" };

typedef struct
{
	int key;
	int down;
} keyq_t;

struct inputdata
{
	Display *x_disp;
	Window x_win;
	int x_shmeventtype;
	void (*eventcallback)(void *eventcallbackdata, int type);
	void *eventcallbackdata;
	
	int config_notify;
	int config_notify_width;
	int config_notify_height;
	
	int gotshmmsg;

	keyq_t keyq[64];
	int keyq_head;
	int keyq_tail;
	
	int mousex;
	int mousey;

	int grabmouse;
	int dga_mouse_enabled;
};

static const unsigned char keytable[] =
{
	0, /* 0 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_ESCAPE,
	'1', /* 10 */
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	'-', /* 20 */
	'=',
	K_BACKSPACE,
	K_TAB,
	'q',
	'w',
	'e',
	'r',
	't',
	'y',
	'u', /* 30 */
	'i',
	'o',
	'p',
	'[',
	']',
	K_ENTER,
	K_LCTRL,
	'a',
	's',
	'd', /* 40 */
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'',
	'`',
	K_LSHIFT, /* 50 */
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.', /* 60 */
	'/',
	K_RSHIFT,
	KP_STAR,
	K_LALT,
	' ',
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3,
	K_F4, /* 70 */
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	KP_NUMLOCK,
	0,
	KP_HOME,
	KP_UPARROW, /* 80 */
	KP_PGUP,
	KP_MINUS,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_PLUS,
	KP_END,
	KP_DOWNARROW,
	KP_PGDN,
	KP_INS, /* 90 */
	KP_DEL,
	0,
	0,
	'<',
	K_F11,
	K_F12,
	K_HOME,
	K_UPARROW,
	K_PGUP,
	K_LEFTARROW, /* 100 */
	0,
	K_RIGHTARROW,
	K_END,
	K_DOWNARROW,
	K_PGDN,
	K_INS,
	K_DEL,
	KP_ENTER,
	K_RCTRL,
	0, /* 110 */
	0,
	KP_SLASH,
	K_RALT,
	0,
	K_LWIN,
	K_RWIN,
	K_MENU,
};

static int XLateKey(XKeyEvent * ev)
{
	int key, kp;
	char buf[64];
	KeySym keysym;

	if (ev->keycode < sizeof(keytable))
	{
		key = keytable[ev->keycode];
		if (key)
			return key;
	}

	key = 0;
	kp = (int) cl_keypad.value;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

	switch (keysym)
	{
		case XK_Scroll_Lock:
			key = K_SCRLCK;
			break;

		case XK_Caps_Lock:
			key = K_CAPSLOCK;
			break;

		case XK_Num_Lock:
			key = kp ? KP_NUMLOCK : K_PAUSE;
			break;

		case XK_KP_Page_Up:
			key = kp ? KP_PGUP : K_PGUP;
			break;
		case XK_Page_Up:
			key = K_PGUP;
			break;

		case XK_KP_Page_Down:
			key = kp ? KP_PGDN : K_PGDN;
			break;
		case XK_Page_Down:
			key = K_PGDN;
			break;

		case XK_KP_Home:
			key = kp ? KP_HOME : K_HOME;
			break;
		case XK_Home:
			key = K_HOME;
			break;

		case XK_KP_End:
			key = kp ? KP_END : K_END;
			break;
		case XK_End:
			key = K_END;
			break;

		case XK_KP_Left:
			key = kp ? KP_LEFTARROW : K_LEFTARROW;
			break;
		case XK_Left:
			key = K_LEFTARROW;
			break;

		case XK_KP_Right:
			key = kp ? KP_RIGHTARROW : K_RIGHTARROW;
			break;
		case XK_Right:
			key = K_RIGHTARROW;
			break;

		case XK_KP_Down:
			key = kp ? KP_DOWNARROW : K_DOWNARROW;
			break;

		case XK_Down:
			key = K_DOWNARROW;
			break;

		case XK_KP_Up:
			key = kp ? KP_UPARROW : K_UPARROW;
			break;

		case XK_Up:
			key = K_UPARROW;
			break;

		case XK_Escape:
			key = K_ESCAPE;
			break;

		case XK_KP_Enter:
			key = kp ? KP_ENTER : K_ENTER;
			break;

		case XK_Return:
			key = K_ENTER;
			break;

		case XK_Tab:
			key = K_TAB;
			break;

		case XK_F1:
			key = K_F1;
			break;

		case XK_F2:
			key = K_F2;
			break;

		case XK_F3:
			key = K_F3;
			break;

		case XK_F4:
			key = K_F4;
			break;

		case XK_F5:
			key = K_F5;
			break;

		case XK_F6:
			key = K_F6;
			break;

		case XK_F7:
			key = K_F7;
			break;

		case XK_F8:
			key = K_F8;
			break;

		case XK_F9:
			key = K_F9;
			break;

		case XK_F10:
			key = K_F10;
			break;

		case XK_F11:
			key = K_F11;
			break;

		case XK_F12:
			key = K_F12;
			break;

		case XK_BackSpace:
			key = K_BACKSPACE;
			break;

		case XK_KP_Delete:
			key = kp ? KP_DEL : K_DEL;
			break;
		case XK_Delete:
			key = K_DEL;
			break;

		case XK_Pause:
			key = K_PAUSE;
			break;

		case XK_Shift_L:
			key = K_LSHIFT;
			break;
		case XK_Shift_R:
			key = K_RSHIFT;
			break;

		case XK_Execute:
		case XK_Control_L:
			key = K_LCTRL;
			break;
		case XK_Control_R:
			key = K_RCTRL;
			break;

		case XK_Alt_L:
		case XK_Meta_L:
			key = K_LALT;
			break;
		case XK_Alt_R:
		case XK_Meta_R:
			key = K_RALT;
			break;

		case XK_Super_L:
			key = K_LWIN;
			break;
		case XK_Super_R:
			key = K_RWIN;
			break;
		case XK_Menu:
			key = K_MENU;
			break;

		case XK_KP_Begin:
			key = kp ? KP_5 : '5';
			break;

		case XK_KP_Insert:
			key = kp ? KP_INS : K_INS;
			break;
		case XK_Insert:
			key = K_INS;
			break;

		case XK_KP_Multiply:
			key = kp ? KP_STAR : '*';
			break;

		case XK_KP_Add:
			key = kp ? KP_PLUS : '+';
			break;

		case XK_KP_Subtract:
			key = kp ? KP_MINUS : '-';
			break;

		case XK_KP_Divide:
			key = kp ? KP_SLASH : '/';
			break;

		default:
			key = *(unsigned char *) buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';

			break;
	}
	return key;
}

static void GetEvents(struct inputdata *id)
{
	XEvent event;

	int newmousex;
	int newmousey;
	
	newmousex = vid.width/2;
	newmousey = vid.height/2;

	do
	{
		XNextEvent(id->x_disp, &event);
		switch (event.type)
		{
			case FocusIn:
			case FocusOut:
				if(id->eventcallback)
					id->eventcallback(id->eventcallbackdata, event.type);
				break;
			case KeyPress:
				id->keyq[id->keyq_head].key = XLateKey(&event.xkey);
				id->keyq[id->keyq_head].down = true;
				id->keyq_head = (id->keyq_head + 1) & 63;
				break;
			case KeyRelease:
				id->keyq[id->keyq_head].key = XLateKey(&event.xkey);
				id->keyq[id->keyq_head].down = false;
				id->keyq_head = (id->keyq_head + 1) & 63;
				break;
			case MotionNotify:
				if (id->dga_mouse_enabled)
				{
					id->mousex+= event.xmotion.x;
					id->mousey+= event.xmotion.y;
				}
				else
				{
					newmousex = event.xmotion.x;
					newmousey = event.xmotion.y;
				}
				break;

			case ButtonPress:
			case ButtonRelease:
				switch (event.xbutton.button)
				{
					case 1:
						Key_Event(K_MOUSE1, event.type == ButtonPress);
						break;
					case 2:
						Key_Event(K_MOUSE3, event.type == ButtonPress);
						break;
					case 3:
						Key_Event(K_MOUSE2, event.type == ButtonPress);
						break;
					case 4:
						Key_Event(K_MWHEELUP, event.type == ButtonPress);
						break;
					case 5:
						Key_Event(K_MWHEELDOWN, event.type == ButtonPress);
						break;
					case 6:
						Key_Event(K_MOUSE4, event.type == ButtonPress);
						break;
					case 7:
						Key_Event(K_MOUSE5, event.type == ButtonPress);
						break;
					case 8:
						Key_Event(K_MOUSE6, event.type == ButtonPress);
						break;
				}
				break;

			case ConfigureNotify:
				id->config_notify_width = event.xconfigure.width;
				id->config_notify_height = event.xconfigure.height;
				id->config_notify = 1;
				break;

			default:
				if(id->x_shmeventtype && event.type == id->x_shmeventtype)
					id->gotshmmsg = 1;
				break;
		}
	} while(XPending(id->x_disp));

	if (!id->dga_mouse_enabled && (newmousex != vid.width/2 || newmousey != vid.height/2))
	{
		newmousex-= vid.width/2;
		newmousey-= vid.height/2;
		id->mousex+= newmousex;
		id->mousey+= newmousey;
	
		if (id->grabmouse)
		{
			/* move the mouse to the window center again */
			XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS&(~PointerMotionMask));
			XWarpPointer(id->x_disp, None, None, 0, 0, 0, 0, -newmousex, -newmousey);
			XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS);
			XFlush(id->x_disp);
		}
	}
}

void X11_Input_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&in_dga_mouse);
	Cvar_ResetCurrentGroup();
}

void *X11_Input_Init(Display *x_disp, Window x_win, int x_shmeventtype, void (*eventcallback)(void *eventcallbackdata, int type), void *eventcallbackdata)
{
	struct inputdata *id;
	id = malloc(sizeof(*id));
	if (id)
	{
		id->x_disp = x_disp;
		id->x_win = x_win;
		id->x_shmeventtype = x_shmeventtype;
		id->eventcallback = eventcallback;
		id->eventcallbackdata = eventcallbackdata;
		id->gotshmmsg = 0;
		id->config_notify = 0;
		id->keyq_head = 0;
		id->keyq_tail = 0;
		id->mousex = 0;
		id->mousey = 0;
		id->grabmouse = 0;
		id->dga_mouse_enabled = 0;

		Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
		Cvar_Register(&cl_keypad);
		Cvar_ResetCurrentGroup();
	}

	return id;
}

void X11_Input_Shutdown(void *inputdata)
{
	X11_Input_GrabMouse(inputdata, 0);

	free(inputdata);
}

void X11_Input_GetEvents(void *inputdata)
{
	struct inputdata *id = inputdata;

	if(XPending(id->x_disp))
		GetEvents(id);

	while(id->keyq_head != id->keyq_tail)
	{
		Key_Event(id->keyq[id->keyq_tail].key, id->keyq[id->keyq_tail].down);
		id->keyq_tail = (id->keyq_tail + 1) & 63;
	}
}

void X11_Input_WaitForShmMsg(void *inputdata)
{
	struct inputdata *id = inputdata;
	
	while(id->gotshmmsg == 0)
		GetEvents(id);
		
	id->gotshmmsg = 0;
}

void X11_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey)
{
	struct inputdata *id = inputdata;

	*mousex = id->mousex;
	*mousey = id->mousey;
	id->mousex = 0;
	id->mousey = 0;
}

void X11_Input_GetConfigNotify(void *inputdata, int *config_notify, int *config_notify_width, int *config_notify_height)
{
	struct inputdata *id = inputdata;

	*config_notify = id->config_notify;
	*config_notify_width = id->config_notify_width;
	*config_notify_height = id->config_notify_height;
	id->config_notify = 0;
}

void X11_Input_GrabMouse(void *inputdata, int dograb)
{
	Window grab_win;
	unsigned int dgaflags;

	struct inputdata *id = inputdata;

	if (dograb && !id->grabmouse)
	{
		/* grab the pointer */
		if (in_dga_mouse.value)
		{
			grab_win = DefaultRootWindow(id->x_disp);
		}
		else
		{
			grab_win = id->x_win;

			XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS&(~PointerMotionMask));
			XWarpPointer(id->x_disp, None, id->x_win, 0, 0, 0, 0, vid.width/2, vid.height/2);
		}

		XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS);

		XGrabPointer(id->x_disp, grab_win, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, grab_win, None, CurrentTime);

		if (in_dga_mouse.value)
		{
			XF86DGAQueryDirectVideo(id->x_disp, DefaultScreen(id->x_disp), &dgaflags);

			if ((dgaflags&XF86DGADirectPresent))
			{
				if (XF86DGADirectVideo(id->x_disp, DefaultScreen(id->x_disp), XF86DGADirectMouse))
				{
					id->dga_mouse_enabled = 1;
				}
			}
		}
	}
	else if (id->grabmouse)
	{
		/* ungrab the pointer */
		if (id->dga_mouse_enabled)
		{
			id->dga_mouse_enabled = 0;
			XF86DGADirectVideo(id->x_disp, DefaultScreen(id->x_disp), 0);
		}
		XUngrabPointer(id->x_disp, CurrentTime);
		XSelectInput(id->x_disp, id->x_win, StructureNotifyMask | KeyPressMask | KeyReleaseMask | ExposureMask | ButtonPressMask | ButtonReleaseMask);
	}

	id->grabmouse = dograb;
}

