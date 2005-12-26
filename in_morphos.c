#include <exec/exec.h>
#include <intuition/intuition.h>
#include <intuition/extensions.h>
#include <intuition/intuitionbase.h>
#include <devices/input.h>

#include <proto/exec.h>
#include <proto/intuition.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#include "in_morphos.h"

struct inputdata
{
	struct Screen *screen;
	struct Window *window;

	struct Interrupt InputHandler;
	struct MsgPort *inputport;
	struct IOStdReq *inputreq;
	BYTE inputopen;

	struct InputEvent imsgs[MAXIMSGS];
	int imsglow;
	int imsghigh;

	int mousex, mousey;
};

#ifndef SA_Displayed
#define SA_Displayed (SA_Dummy + 101)
#endif

extern struct IntuitionBase *IntuitionBase;

extern cvar_t _windowed_mouse;

#define DEBUGRING(x)

void Sys_Input_Shutdown(void *inputdata)
{
	struct inputdata *id = inputdata;

	if (id->inputopen)
	{
		id->inputreq->io_Data = (void *)&id->InputHandler;
		id->inputreq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)id->inputreq);

		CloseDevice((struct IORequest *)id->inputreq);
	}

	if (id->inputreq)
		DeleteIORequest(id->inputreq);

	if (id->inputport)
		DeleteMsgPort(id->inputport);

	FreeVec(id);
}

void *Sys_Input_Init(struct Screen *screen, struct Window *window)
{
	struct inputdata *id;

	id = AllocVec(sizeof(*id), MEMF_ANY);
	if (id)
	{
		id->screen = screen;
		id->window = window;

		id->inputport = CreateMsgPort();
		if (id->inputport == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to create message port");
		}

		id->inputreq = CreateIORequest(id->inputport, sizeof(*id->inputreq));
		if (id->inputreq == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to create IO request");
		}

		id->inputopen = !OpenDevice("input.device", 0, (struct IORequest *)id->inputreq, 0);
		if (id->inputopen == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to open input.device");
		}

		id->InputHandler.is_Node.ln_Type = NT_INTERRUPT;
		id->InputHandler.is_Node.ln_Pri = 100;
		id->InputHandler.is_Node.ln_Name = "FodQuake input handler";
		id->InputHandler.is_Data = id;
		id->InputHandler.is_Code = (void (*)())&myinputhandler;
		id->inputreq->io_Data = (void *)&id->InputHandler;
		id->inputreq->io_Command = IND_ADDHANDLER;
		DoIO((struct IORequest *)id->inputreq);
	}

	return id;
}

static void ExpireRingBuffer(struct inputdata *id)
{
	int i = 0;

	while (id->imsgs[id->imsglow].ie_Class == IECLASS_NULL && id->imsglow != id->imsghigh)
	{
		id->imsglow++;
		i++;
		id->imsglow %= MAXIMSGS;
	}

	DEBUGRING(dprintf("Expired %d messages\n", i));
}

void Sys_Input_GetEvents(void *inputdata)
{
	struct inputdata *id = inputdata;
	char key;
	int i;
	int down;

	for (i = id->imsglow; i != id->imsghigh; i++, i %= MAXIMSGS)
	{
		DEBUGRING(dprintf("%d %d\n", i, id->imsghigh));
		if (id->imsgs[i].ie_Class == IECLASS_NEWMOUSE)
		{
			key = 0;

			if (id->imsgs[i].ie_Code == NM_WHEEL_UP)
				key = K_MWHEELUP;
			else if (id->imsgs[i].ie_Code == NM_WHEEL_DOWN)
				key = K_MWHEELDOWN;

			if (id->imsgs[i].ie_Code == NM_BUTTON_FOURTH)
			{
				Key_Event(K_MOUSE4, true);
			}
			else if (id->imsgs[i].ie_Code == (NM_BUTTON_FOURTH | IECODE_UP_PREFIX))
			{
				Key_Event(K_MOUSE4, false);
			}

			if (key)
			{
				Key_Event(key, 1);
				Key_Event(key, 0);
			}
		}
		else if (id->imsgs[i].ie_Class == IECLASS_RAWKEY)
		{
			down = !(id->imsgs[i].ie_Code & IECODE_UP_PREFIX);
			id->imsgs[i].ie_Code &= ~IECODE_UP_PREFIX;

			key = 0;
			if (id->imsgs[i].ie_Code <= 255)
				key = keyconv[id->imsgs[i].ie_Code];

			if (key)
				Key_Event(key, down);
			else
			{
				if (developer.value)
					Com_Printf("Unknown key %d\n", id->imsgs[i].ie_Code);
			}
		}
		else if (id->imsgs[i].ie_Class == IECLASS_RAWMOUSE)
		{
			if (id->imsgs[i].ie_Code == IECODE_LBUTTON)
				Key_Event(K_MOUSE1, true);
			else if (id->imsgs[i].ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
				Key_Event(K_MOUSE1, false);
			else if (id->imsgs[i].ie_Code == IECODE_RBUTTON)
				Key_Event(K_MOUSE2, true);
			else if (id->imsgs[i].ie_Code == (IECODE_RBUTTON | IECODE_UP_PREFIX))
				Key_Event(K_MOUSE2, false);
			else if (id->imsgs[i].ie_Code == IECODE_MBUTTON)
				Key_Event(K_MOUSE3, true);
			else if (id->imsgs[i].ie_Code == (IECODE_MBUTTON | IECODE_UP_PREFIX))
				Key_Event(K_MOUSE3, false);

			id->mousex += id->imsgs[i].ie_position.ie_xy.ie_x;
			id->mousey += id->imsgs[i].ie_position.ie_xy.ie_y;
		}

		id->imsgs[i].ie_Class = IECLASS_NULL;
	}

	ExpireRingBuffer(id);
}

void Sys_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey)
{
	struct inputdata *id = inputdata;

	*mousex = id->mousex;
	*mousey = id->mousey;
	id->mousex = 0;
	id->mousey = 0;
}

static char keyconv[] = {
	'`',			/* 0 */
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',			/* 10 */
	'-',
	'=',
	0,
	0,
	K_INS,
	'q',
	'w',
	'e',
	'r',
	't',			/* 20 */
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	0,
	KP_END,
	KP_DOWNARROW,		/* 30 */
	KP_PGDN,
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',			/* 40 */
	';',
	'\'',
	'\\',
	0,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	'<',
	'z',
	'x',			/* 50 */
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/',
	0,
	KP_DEL,			/* 60 */
	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	' ',
	K_BACKSPACE,
	K_TAB,
	KP_ENTER,
	K_ENTER,
	K_ESCAPE,
	K_DEL,			/* 70 */
	K_INS,
	K_PGUP,
	K_PGDN,
	KP_MINUS,
	K_F11,
	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,
	K_F1,			/* 80 */
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	0,			/* 90 */
	0,
	KP_SLASH,
	0,
	KP_PLUS,
	0,
	K_LSHIFT,
	K_RSHIFT,
	0,
	K_RCTRL,
	K_ALT,			/* 100 */
	K_ALT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_PAUSE,		/* 110 */
	K_F12,
	K_HOME,
	K_END,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 120 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 150 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 180 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 220 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 230 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 250 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

static struct InputEvent *myinputhandler_real(void);

static struct EmulLibEntry myinputhandler =
{
	TRAP_LIB, 0, (void (*)(void))myinputhandler_real
};

static struct InputEvent *myinputhandler_real()
{
	struct InputEvent *moo = (struct InputEvent *)REG_A0;
	struct inputdata *id = (struct inputdata *)REG_A1;

	struct InputEvent *coin;

	int screeninfront;

	if (!(id->window->Flags & WFLG_WINDOWACTIVE))
		return moo;

	coin = moo;

	if (id->screen)
	{
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 56))
			GetAttr(SA_Displayed, id->screen, &screeninfront);
		else
			screeninfront = id->screen == IntuitionBase->FirstScreen;
	}
	else
		screeninfront = 1;

	do
	{
		if (coin->ie_Class == IECLASS_RAWMOUSE || coin->ie_Class == IECLASS_RAWKEY || coin->ie_Class == IECLASS_NEWMOUSE)
		{
			if ((id->imsghigh > id->imsglow && !(id->imsghigh == MAXIMSGS - 1 && id->imsglow == 0)) || (id->imsghigh < id->imsglow && id->imsghigh != id->imsglow - 1) || id->imsglow == id->imsghigh)
			{
				memcpy(&id->imsgs[id->imsghigh], coin, sizeof(id->imsgs[0]));
				id->imsghigh++;
				id->imsghigh %= MAXIMSGS;
			}
			else
			{
				DEBUGRING(kprintf("FodQuake: message dropped, imsglow = %d, imsghigh = %d\n", id->imsglow, id->imsghigh));
			}

			if ((id->window->Flags & WFLG_WINDOWACTIVE) && coin->ie_Class == IECLASS_RAWMOUSE && screeninfront && id->window->MouseX > 0 && id->window->MouseY > 0)
			{
				if (_windowed_mouse.value)
				{
					coin->ie_position.ie_xy.ie_x = 0;
					coin->ie_position.ie_xy.ie_y = 0;
				}
			}
		}

		coin = coin->ie_NextEvent;
	} while (coin);

	return moo;
}

