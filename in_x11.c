#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#define XINPUTFLAGS (StructureNotifyMask|KeyPressMask|KeyReleaseMask|ExposureMask|ButtonPressMask|ButtonReleaseMask|FocusChangeMask)

cvar_t cl_keypad = { "cl_keypad", "1" };

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
};

static int XLateKey(XKeyEvent * ev)
{
	int key, kp;
	char buf[64];
	KeySym keysym;

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
//                      fprintf(stdout, "case 0x0%x: key = ___;break;/* [%c] */\n", keysym);
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
				newmousex = event.xmotion.x;
				newmousey = event.xmotion.y;
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

	if (newmousex != vid.width/2 || newmousey != vid.height/2)
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

		Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
		Cvar_Register(&cl_keypad);
		Cvar_ResetCurrentGroup();
	}

	return id;
}

void X11_Input_Shutdown(void *inputdata)
{
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
	struct inputdata *id = inputdata;

	id->grabmouse = dograb;

	if (dograb)
	{
		/* grab the pointer */
		XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS&(~PointerMotionMask));
		XWarpPointer(id->x_disp, None, id->x_win, 0, 0, 0, 0, vid.width/2, vid.height/2);

		XGrabPointer(id->x_disp, id->x_win, True, 0, GrabModeAsync, GrabModeAsync, id->x_win, None, CurrentTime);
		XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS);
	}
	else
	{
		/* ungrab the pointer */
		XUngrabPointer(id->x_disp, CurrentTime);
		XSelectInput(id->x_disp, id->x_win, StructureNotifyMask | KeyPressMask | KeyReleaseMask | ExposureMask | ButtonPressMask | ButtonReleaseMask);
	}
}

