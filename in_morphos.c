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

#ifndef SA_Displayed
#define SA_Displayed (SA_Dummy + 101)
#endif

struct InputEvent imsgs[MAXIMSGS];
extern struct IntuitionBase *IntuitionBase;
extern struct Window *window;
extern struct Screen *screen;

int imsglow = 0;
int imsghigh = 0;

extern qboolean mouse_active;

static struct Interrupt InputHandler;

static struct Interrupt InputHandler;
static struct MsgPort *inputport = 0;
static struct IOStdReq *inputreq = 0;
static BYTE inputret = -1;

cvar_t m_filter = {"m_filter", "1", CVAR_ARCHIVE};

extern cvar_t _windowed_mouse;

float mouse_x, mouse_y;
float old_mouse_x, old_mouse_y;

#define DEBUGRING(x)

void IN_Shutdown(void)
{
	if (inputret == 0)
	{
		inputreq->io_Data = (void *)&InputHandler;
		inputreq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)inputreq);

		CloseDevice((struct IORequest *)inputreq);

		inputret = -1;
	}

	if (inputreq)
	{
		DeleteIORequest(inputreq);

		inputreq = 0;
	}

	if (inputport)
	{
		DeleteMsgPort(inputport);

		inputport = 0;
	}

}

void IN_Init()
{
	inputport = CreateMsgPort();
	if (inputport == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create message port");
	}

	inputreq = CreateIORequest(inputport, sizeof(*inputreq));
	if (inputreq == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create IO request");
	}

	inputret = OpenDevice("input.device", 0, (struct IORequest *)inputreq, 0);
	if (inputret != 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to open input.device");
	}

	InputHandler.is_Node.ln_Type = NT_INTERRUPT;
	InputHandler.is_Node.ln_Pri = 100;
	InputHandler.is_Node.ln_Name = "Fuhquake input handler";
	InputHandler.is_Data = 0;
	InputHandler.is_Code = (void(*)())&myinputhandler;
	inputreq->io_Data = (void *)&InputHandler;
	inputreq->io_Command = IND_ADDHANDLER;
	DoIO((struct IORequest *)inputreq);

}

static void ExpireRingBuffer()
{
	int i = 0;

	while(imsgs[imsglow].ie_Class == IECLASS_NULL && imsglow != imsghigh)
	{
		imsglow++;
		i++;
		imsglow%= MAXIMSGS;
	}

	DEBUGRING(dprintf("Expired %d messages\n", i));
}

void IN_Commands(void)
{
	char key;
	int i;
	int down;
	struct InputEvent ie;

	for(i = imsglow;i != imsghigh;i++, i%= MAXIMSGS)
	{
		DEBUGRING(dprintf("%d %d\n", i, imsghigh));
		if (imsgs[i].ie_Class == IECLASS_NEWMOUSE)
		{
			if ((window->Flags & WFLG_WINDOWACTIVE))
			{
				key = 0;

				if (imsgs[i].ie_Code == NM_WHEEL_UP)
					key = K_MWHEELUP;
				else if (imsgs[i].ie_Code == NM_WHEEL_DOWN)
					key = K_MWHEELDOWN;

				if (imsgs[i].ie_Code == NM_BUTTON_FOURTH)
				{
					Key_Event(K_MOUSE4, true);
				}
				else if (imsgs[i].ie_Code == (NM_BUTTON_FOURTH|IECODE_UP_PREFIX))
				{
					Key_Event(K_MOUSE4, false);
				}

				if (key)
				{
					Key_Event(key, 1);
					Key_Event(key, 0);
				}

			}

			imsgs[i].ie_Class = IECLASS_NULL;
		}	
		else if (imsgs[i].ie_Class == IECLASS_RAWKEY)
		{
			if ((window->Flags & WFLG_WINDOWACTIVE))
			{
				down = !(imsgs[i].ie_Code&IECODE_UP_PREFIX);
				imsgs[i].ie_Code&=~IECODE_UP_PREFIX;

				memcpy(&ie, &imsgs[i], sizeof(ie));

				key = 0;
				if (imsgs[i].ie_Code <= 255)
					key = keyconv[imsgs[i].ie_Code];

				if (key)
					Key_Event(key, down);
				else
				{
					if (developer.value)
						Com_Printf("Unknown key %d\n", imsgs[i].ie_Code);
				}
			}

			imsgs[i].ie_Class = IECLASS_NULL;
		}
		else if (imsgs[i].ie_Class == IECLASS_RAWMOUSE)
		{
			if ((window->Flags & WFLG_WINDOWACTIVE))
			{
				if (imsgs[i].ie_Code == IECODE_LBUTTON)
					Key_Event(K_MOUSE1, true);
				else if (imsgs[i].ie_Code == (IECODE_LBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE1, false);
				else if (imsgs[i].ie_Code == IECODE_RBUTTON)
					Key_Event(K_MOUSE2, true);
				else if (imsgs[i].ie_Code == (IECODE_RBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE2, false);
				else if (imsgs[i].ie_Code == IECODE_MBUTTON)
					Key_Event(K_MOUSE3, true);
				else if (imsgs[i].ie_Code == (IECODE_MBUTTON|IECODE_UP_PREFIX))
					Key_Event(K_MOUSE3, false);

				mouse_x+= imsgs[i].ie_position.ie_xy.ie_x;
				mouse_y+= imsgs[i].ie_position.ie_xy.ie_y;
			}

			imsgs[i].ie_Class = IECLASS_NULL;
		}

	}

	ExpireRingBuffer();

}

void IN_Move(usercmd_t *cmd)
{
	float tx, ty, filterfrac;

	tx = mouse_x;
	ty = mouse_y;

	if (m_filter.value)
	{
		filterfrac = bound(0, m_filter.value, 1) / 2.0;
	        mouse_x = (tx * (1 - filterfrac) + old_mouse_x * filterfrac);
        	mouse_y = (ty * (1 - filterfrac) + old_mouse_y * filterfrac);
	}

	old_mouse_x = tx;
	old_mouse_y = ty;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mlook_active)
		V_StopPitchDrift ();

	if (mlook_active && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
	}
	else
	{
		cmd->forwardmove -= m_forward.value * mouse_y;
	}

	mouse_x = mouse_y = 0.0;
}

char keyconv[] =
{
	'`', /* 0 */
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0', /* 10 */
	'-',
	'=',
	0,
	0,
	K_INS,
	'q',
	'w',
	'e',
	'r',
	't', /* 20 */
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	0,
	KP_END,
	KP_DOWNARROW, /* 30 */
	KP_PGDN,
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l', /* 40 */
	';',
	'\'',
	'\\',
	0,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	'<',
	'z',
	'x', /* 50 */
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/',
	0,
	KP_DEL, /* 60 */
	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	' ',
	K_BACKSPACE,
	K_TAB,
	KP_ENTER,
	K_ENTER,
	K_ESCAPE,
	K_DEL, /* 70 */
	K_INS,
	K_PGUP,
	K_PGDN,
	KP_MINUS,
	K_F11,
	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,
	K_F1, /* 80 */
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	0, /* 90 */
	0,
	KP_SLASH,
	0,
	KP_PLUS,
	0,
	K_LSHIFT,
	K_RSHIFT,
	0,
	K_RCTRL,
	K_ALT, /* 100 */
	K_ALT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_PAUSE, /* 110 */
	K_F12,
	K_HOME,
	K_END,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 120 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 150 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 180 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 220 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 230 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 250 */
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

struct InputEvent *myinputhandler_real(void);

struct EmulLibEntry myinputhandler =
{
	TRAP_LIB, 0, (void(*)(void))myinputhandler_real
};

struct InputEvent *myinputhandler_real()
{
	struct InputEvent *moo = (struct InputEvent *)REG_A0;

	struct InputEvent *coin;

	int screeninfront;

	coin = moo;

	if (screen)
	{
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 56))
			GetAttr(SA_Displayed, screen, &screeninfront);
		else
			screeninfront = screen==IntuitionBase->FirstScreen;
	}
	else
		screeninfront = 1;

	do
	{
		if (coin->ie_Class == IECLASS_RAWMOUSE || coin->ie_Class == IECLASS_RAWKEY || coin->ie_Class == IECLASS_NEWMOUSE)
		{
			if ((imsghigh > imsglow && !(imsghigh == MAXIMSGS-1 && imsglow == 0)) || (imsghigh < imsglow && imsghigh != imsglow-1) || imsglow == imsghigh)
			{
				memcpy(&imsgs[imsghigh], coin, sizeof(imsgs[0]));
				imsghigh++;
				imsghigh%= MAXIMSGS;
			}
			else
			{
				DEBUGRING(kprintf("FodQuake: message dropped, imsglow = %d, imsghigh = %d\n", imsglow, imsghigh));
			}

			if (/*mouse_active && */(window->Flags & WFLG_WINDOWACTIVE) && coin->ie_Class == IECLASS_RAWMOUSE && screeninfront && window->MouseX > 0 && window->MouseY > 0)
			{
				if (_windowed_mouse.value)
				{
					coin->ie_position.ie_xy.ie_x = 0;
					coin->ie_position.ie_xy.ie_y = 0;
				}
			}
		}

		coin = coin->ie_NextEvent;
	} while(coin);

	return moo;
}

