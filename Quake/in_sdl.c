/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

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

#include "quakedef.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif
#else
#include "SDL.h"
#endif

static qboolean	prev_gamekey, gamekey;

#ifdef __APPLE__
/* Mouse acceleration needs to be disabled on OS X */
#define MACOS_X_ACCELERATION_HACK
#endif

#ifdef MACOS_X_ACCELERATION_HACK
#include <IOKit/IOTypes.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
#endif

static qboolean	no_mouse = false;

static int buttonremap[] =
{
	K_MOUSE1,
	K_MOUSE3,	/* right button		*/
	K_MOUSE2,	/* middle button	*/
	K_MWHEELUP,
	K_MWHEELDOWN,
	K_MOUSE4,
	K_MOUSE5
};

/* mouse variables */
cvar_t	m_filter = {"m_filter","0",CVAR_NONE};

/* total accumulated mouse movement since last frame,
 * gets updated from the main game loop via IN_MouseMove */
static int	total_dx, total_dy = 0;

static int IN_FilterMouseEvents (const SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
	// case SDL_MOUSEBUTTONDOWN:
	// case SDL_MOUSEBUTTONUP:
		return 0;
	}

	return 1;
}

#if defined(USE_SDL2)
static int IN_SDL2_FilterMouseEvents (void *userdata, SDL_Event *event)
{
	return IN_FilterMouseEvents (event);
}
#endif

static void IN_BeginIgnoringMouseEvents()
{
#if defined(USE_SDL2)
	SDL_EventFilter currentFilter = NULL;
	void *currentUserdata = NULL;
	SDL_GetEventFilter(&currentFilter, &currentUserdata);

	if (currentFilter != IN_SDL2_FilterMouseEvents)
		SDL_SetEventFilter(IN_SDL2_FilterMouseEvents, NULL);
#else
	if (SDL_GetEventFilter() != IN_FilterMouseEvents)
		SDL_SetEventFilter(IN_FilterMouseEvents);
#endif
}

static void IN_EndIgnoringMouseEvents()
{
#if defined(USE_SDL2)
	SDL_EventFilter currentFilter;
	void *currentUserdata;
	if (SDL_GetEventFilter(&currentFilter, &currentUserdata) == SDL_TRUE)
		SDL_SetEventFilter(NULL, NULL);
#else
	if (SDL_GetEventFilter() != NULL)
		SDL_SetEventFilter(NULL);
#endif
}

#ifdef MACOS_X_ACCELERATION_HACK
static cvar_t in_disablemacosxmouseaccel = {"in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE};
static double originalMouseSpeed = -1.0;

static io_connect_t IN_GetIOHandle(void)
{
	io_connect_t iohandle = MACH_PORT_NULL;
	io_service_t iohidsystem = MACH_PORT_NULL;
	mach_port_t masterport;
	kern_return_t status;

	status = IOMasterPort(MACH_PORT_NULL, &masterport);
	if (status != KERN_SUCCESS)
		return 0;

	iohidsystem = IORegistryEntryFromPath(masterport, kIOServicePlane ":/IOResources/IOHIDSystem");
	if (!iohidsystem)
		return 0;

	status = IOServiceOpen(iohidsystem, mach_task_self(), kIOHIDParamConnectType, &iohandle);
	IOObjectRelease(iohidsystem);

	return iohandle;
}

static void IN_DisableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &originalMouseSpeed) == kIOReturnSuccess)
		{
			if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
			{
				Cvar_Set("in_disablemacosxmouseaccel", "0");
				Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
			}
		}
		else
		{
			Cvar_Set("in_disablemacosxmouseaccel", "0");
			Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
		}
		IOServiceClose(mouseDev);
	}
	else
	{
		Cvar_Set("in_disablemacosxmouseaccel", "0");
		Con_Printf("WARNING: Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
}

static void IN_ReenableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), originalMouseSpeed) != kIOReturnSuccess)
			Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
		IOServiceClose(mouseDev);
	}
	else
	{
		Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
	originalMouseSpeed = -1;
}
#endif /* MACOS_X_ACCELERATION_HACK */


void IN_Activate (void)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	/* Save the status of mouse acceleration */
	if (originalMouseSpeed == -1 && in_disablemacosxmouseaccel.value)
		IN_DisableOSXMouseAccel();
#endif

#if defined(USE_SDL2)
	if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
	{
		Con_Printf("WARNING: SDL_SetRelativeMouseMode(SDL_TRUE) failed.\n");
	}
#else
	if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_ON)
	{
		SDL_WM_GrabInput(SDL_GRAB_ON);
		if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_ON)
			Con_Printf("WARNING: SDL_WM_GrabInput(SDL_GRAB_ON) failed.\n");
	}

	if (SDL_ShowCursor(SDL_QUERY) != SDL_DISABLE)
	{
		SDL_ShowCursor(SDL_DISABLE);
		if (SDL_ShowCursor(SDL_QUERY) != SDL_DISABLE)
			Con_Printf("WARNING: SDL_ShowCursor(SDL_DISABLE) failed.\n");
	}
#endif

	IN_EndIgnoringMouseEvents();

	total_dx = 0;
	total_dy = 0;
}

void IN_Deactivate (qboolean free_cursor)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	if (originalMouseSpeed != -1)
		IN_ReenableOSXMouseAccel();
#endif

	if (free_cursor)
	{
#if defined(USE_SDL2)
		SDL_SetRelativeMouseMode(SDL_FALSE);
#else
		if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_OFF)
		{
			SDL_WM_GrabInput(SDL_GRAB_OFF);
			if (SDL_WM_GrabInput(SDL_GRAB_QUERY) != SDL_GRAB_OFF)
				Con_Printf("WARNING: SDL_WM_GrabInput(SDL_GRAB_OFF) failed.\n");
		}

		if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
		{
			SDL_ShowCursor(SDL_ENABLE);
			if (SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
				Con_Printf("WARNING: SDL_ShowCursor(SDL_ENABLE) failed.\n");
		}
#endif
	}

	/* discard all mouse events when input is deactivated */
	IN_BeginIgnoringMouseEvents();
}

void IN_Init (void)
{
	prev_gamekey = Key_GameKey();

#if !defined(USE_SDL2)
	SDL_EnableUNICODE (!prev_gamekey);
	if (SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL) == -1)
		Con_Printf("Warning: SDL_EnableKeyRepeat() failed.\n");
#endif
	if (safemode || COM_CheckParm("-nomouse"))
	{
		no_mouse = true;
		/* discard all mouse events when input is deactivated */
		IN_BeginIgnoringMouseEvents();
	}

#ifdef MACOS_X_ACCELERATION_HACK
	Cvar_RegisterVariable(&in_disablemacosxmouseaccel);
#endif

	IN_Activate();
}

void IN_Shutdown (void)
{
	IN_Deactivate(true);
}

void IN_Commands (void)
{
/* TODO: implement this for joystick support */
}

extern cvar_t cl_maxpitch; /* johnfitz -- variable pitch clamping */
extern cvar_t cl_minpitch; /* johnfitz -- variable pitch clamping */


void IN_MouseMove(int dx, int dy)
{
	total_dx += dx;
	total_dy += dy;
}

void IN_Move (usercmd_t *cmd)
{
	int		dmx, dmy;

	/* TODO: fix this
	if (m_filter.value)
	{
		dmx = (2*mx - dmx) * 0.5;
		dmy = (2*my - dmy) * 0.5;
	}
	*/

	dmx = total_dx * sensitivity.value;
	dmy = total_dy * sensitivity.value;

	total_dx = 0;
	total_dy = 0;

	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * dmx;
	else
		cl.viewangles[YAW] -= m_yaw.value * dmx;

	if (in_mlook.state & 1)
	{
		if (dmx || dmy)
			V_StopPitchDrift ();
	}

	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * dmy;
		/* johnfitz -- variable pitch clamping */
		if (cl.viewangles[PITCH] > cl_maxpitch.value)
			cl.viewangles[PITCH] = cl_maxpitch.value;
		if (cl.viewangles[PITCH] < cl_minpitch.value)
			cl.viewangles[PITCH] = cl_minpitch.value;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * dmy;
		else
			cmd->forwardmove -= m_forward.value * dmy;
	}
}

void IN_ClearStates (void)
{
}

void IN_UpdateForKeydest (void)
{
	gamekey = Key_GameKey();
	if (gamekey != prev_gamekey)
	{
		prev_gamekey = gamekey;
		Key_ClearStates();
#if !defined(USE_SDL2)
		SDL_EnableUNICODE(!gamekey);
#else
		if (gamekey)
			SDL_StopTextInput();
		else
			SDL_StartTextInput();
#endif
	}
}

#if defined(USE_SDL2)
static inline int IN_SDL2_ScancodeToQuakeKey(SDL_Scancode scancode)
{
	switch (scancode)
	{
		case SDL_SCANCODE_TAB: return K_TAB;
		case SDL_SCANCODE_RETURN: return K_ENTER;
		case SDL_SCANCODE_RETURN2: return K_ENTER;
		case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
		case SDL_SCANCODE_SPACE: return K_SPACE;

		case SDL_SCANCODE_A: return 'a';
		case SDL_SCANCODE_B: return 'b';
		case SDL_SCANCODE_C: return 'c';
		case SDL_SCANCODE_D: return 'd';
		case SDL_SCANCODE_E: return 'e';
		case SDL_SCANCODE_F: return 'f';
		case SDL_SCANCODE_G: return 'g';
		case SDL_SCANCODE_H: return 'h';
		case SDL_SCANCODE_I: return 'i';
		case SDL_SCANCODE_J: return 'j';
		case SDL_SCANCODE_K: return 'k';
		case SDL_SCANCODE_L: return 'l';
		case SDL_SCANCODE_M: return 'm';
		case SDL_SCANCODE_N: return 'n';
		case SDL_SCANCODE_O: return 'o';
		case SDL_SCANCODE_P: return 'p';
		case SDL_SCANCODE_Q: return 'q';
		case SDL_SCANCODE_R: return 'r';
		case SDL_SCANCODE_S: return 's';
		case SDL_SCANCODE_T: return 't';
		case SDL_SCANCODE_U: return 'u';
		case SDL_SCANCODE_V: return 'v';
		case SDL_SCANCODE_W: return 'w';
		case SDL_SCANCODE_X: return 'x';
		case SDL_SCANCODE_Y: return 'y';
		case SDL_SCANCODE_Z: return 'z';

		case SDL_SCANCODE_1: return '1';
		case SDL_SCANCODE_2: return '2';
		case SDL_SCANCODE_3: return '3';
		case SDL_SCANCODE_4: return '4';
		case SDL_SCANCODE_5: return '5';
		case SDL_SCANCODE_6: return '6';
		case SDL_SCANCODE_7: return '7';
		case SDL_SCANCODE_8: return '8';
		case SDL_SCANCODE_9: return '9';
		case SDL_SCANCODE_0: return '0';

		case SDL_SCANCODE_MINUS: return '-';
		case SDL_SCANCODE_EQUALS: return '=';
		case SDL_SCANCODE_LEFTBRACKET: return '[';
		case SDL_SCANCODE_RIGHTBRACKET: return ']';
		case SDL_SCANCODE_BACKSLASH: return '\\';
		case SDL_SCANCODE_NONUSHASH: return '#';
		case SDL_SCANCODE_SEMICOLON: return ';';
		case SDL_SCANCODE_APOSTROPHE: return '\'';
		case SDL_SCANCODE_GRAVE: return '`';
		case SDL_SCANCODE_COMMA: return ',';
		case SDL_SCANCODE_PERIOD: return '.';
		case SDL_SCANCODE_SLASH: return '/';
		case SDL_SCANCODE_NONUSBACKSLASH: return '\\';

		case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;
		case SDL_SCANCODE_UP: return K_UPARROW;
		case SDL_SCANCODE_DOWN: return K_DOWNARROW;
		case SDL_SCANCODE_LEFT: return K_LEFTARROW;
		case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;

		case SDL_SCANCODE_LALT: return K_ALT;
		case SDL_SCANCODE_RALT: return K_ALT;
		case SDL_SCANCODE_LCTRL: return K_CTRL;
		case SDL_SCANCODE_RCTRL: return K_CTRL;
		case SDL_SCANCODE_LSHIFT: return K_SHIFT;
		case SDL_SCANCODE_RSHIFT: return K_SHIFT;

		case SDL_SCANCODE_F1: return K_F1;
		case SDL_SCANCODE_F2: return K_F2;
		case SDL_SCANCODE_F3: return K_F3;
		case SDL_SCANCODE_F4: return K_F4;
		case SDL_SCANCODE_F5: return K_F5;
		case SDL_SCANCODE_F6: return K_F6;
		case SDL_SCANCODE_F7: return K_F7;
		case SDL_SCANCODE_F8: return K_F8;
		case SDL_SCANCODE_F9: return K_F9;
		case SDL_SCANCODE_F10: return K_F10;
		case SDL_SCANCODE_F11: return K_F11;
		case SDL_SCANCODE_F12: return K_F12;
		case SDL_SCANCODE_INSERT: return K_INS;
		case SDL_SCANCODE_DELETE: return K_DEL;
		case SDL_SCANCODE_PAGEDOWN: return K_PGDN;
		case SDL_SCANCODE_PAGEUP: return K_PGUP;
		case SDL_SCANCODE_HOME: return K_HOME;
		case SDL_SCANCODE_END: return K_END;

		case SDL_SCANCODE_NUMLOCKCLEAR: return K_KP_NUMLOCK;
		case SDL_SCANCODE_KP_DIVIDE: return K_KP_SLASH;
		case SDL_SCANCODE_KP_MULTIPLY: return K_KP_STAR;
		case SDL_SCANCODE_KP_MINUS: return K_KP_MINUS;
		case SDL_SCANCODE_KP_7: return K_KP_HOME;
		case SDL_SCANCODE_KP_8: return K_KP_UPARROW;
		case SDL_SCANCODE_KP_9: return K_KP_PGUP;
		case SDL_SCANCODE_KP_PLUS: return K_KP_PLUS;
		case SDL_SCANCODE_KP_4: return K_KP_LEFTARROW;
		case SDL_SCANCODE_KP_5: return K_KP_5;
		case SDL_SCANCODE_KP_6: return K_KP_RIGHTARROW;
		case SDL_SCANCODE_KP_1: return K_KP_END;
		case SDL_SCANCODE_KP_2: return K_KP_DOWNARROW;
		case SDL_SCANCODE_KP_3: return K_KP_PGDN;
		case SDL_SCANCODE_KP_ENTER: return K_KP_ENTER;
		case SDL_SCANCODE_KP_0: return K_KP_INS;
		case SDL_SCANCODE_KP_PERIOD: return K_KP_DEL;

		case SDL_SCANCODE_LGUI: return K_COMMAND;
		case SDL_SCANCODE_RGUI: return K_COMMAND;

		case SDL_SCANCODE_PAUSE: return K_PAUSE;

		default: return 0;
	}
}
#endif

void IN_SendKeyEvents (void)
{
	SDL_Event event;
	int sym;
#if defined(USE_SDL2)
	static int lastKeyDown = 0;
#else
	int state, modstate;
#endif
	
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
#if defined(USE_SDL2)
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				S_UnblockSound();
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				S_BlockSound();
			break;
#else
		case SDL_ACTIVEEVENT:
			if (event.active.state & (SDL_APPINPUTFOCUS|SDL_APPACTIVE))
			{
				if (event.active.gain)
					S_UnblockSound();
				else	S_BlockSound();
			}
			break;
#endif
#if defined(USE_SDL2)
		case SDL_TEXTINPUT:
		// SDL2: We use SDL_TEXTINPUT for typing in the console / chat.
		// SDL2 uses the local keyboard layout and handles modifiers
		// (shift for uppercase, etc.) for us.
			if (!Key_ConsoleBindable(lastKeyDown))
			{
				unsigned char *ch;
				for (ch = (unsigned char *)event.text.text; ch[0] != 0 && ch[0] < 128; ch++)
					Char_Event (ch[0]);
			}
			break;
#endif
		case SDL_KEYDOWN:
			if ((event.key.keysym.sym == SDLK_RETURN) &&
			    (event.key.keysym.mod & KMOD_ALT))
			{
				VID_Toggle();
				break;
			}
			if ((event.key.keysym.sym == SDLK_ESCAPE) &&
			    (event.key.keysym.mod & KMOD_SHIFT))
			{
				Con_ToggleConsole_f();
				break;
			}
		/* fallthrough */
		case SDL_KEYUP:
#if defined(USE_SDL2)
		// SDL2: in gamekey mode, we interpret the keyboard as the US
		// layout, so keybindings are based on key position, not the label
		// on the key cap.
			sym = IN_SDL2_ScancodeToQuakeKey(event.key.keysym.scancode);

			if (event.type == SDL_KEYDOWN)
				lastKeyDown = sym;
			else
				lastKeyDown = 0;
			
			Key_Event (sym, event.key.state == SDL_PRESSED);
			break;
#else
			sym = event.key.keysym.sym;
			state = event.key.state;
			modstate = SDL_GetModState();

			if (event.key.keysym.unicode != 0)
			{
				if ((event.key.keysym.unicode & 0xFF80) == 0)
				{
					int usym = event.key.keysym.unicode & 0x7F;
					if (modstate & KMOD_CTRL && usym < 32 && sym >= 32)
					{
						/* control characters */
						if (modstate & KMOD_SHIFT)
							usym += 64;
						else	usym += 96;
					}
#if defined(__APPLE__) && defined(__MACH__)
					if (sym == SDLK_BACKSPACE)
						usym = sym;	/* avoid change to SDLK_DELETE */
#endif	/* Mac OS X */
#if defined(__QNX__) || defined(__QNXNTO__)
					if (sym == SDLK_BACKSPACE || sym == SDLK_RETURN)
						usym = sym;	/* S.A: fixes QNX weirdness */
#endif	/* __QNX__ */
					/* only use unicode for ` and ~ in game mode */
					if (!gamekey || usym == '`' || usym == '~')
						sym = usym;
				}
				/* else: it's an international character */
			}
			/*printf("You pressed %s (%d) (%c)\n", SDL_GetKeyName(sym), sym, sym);*/

			switch (sym)
			{
			case SDLK_DELETE:
				sym = K_DEL;
				break;
			case SDLK_BACKSPACE:
				sym = K_BACKSPACE;
				break;
			case SDLK_F1:
				sym = K_F1;
				break;
			case SDLK_F2:
				sym = K_F2;
				break;
			case SDLK_F3:
				sym = K_F3;
				break;
			case SDLK_F4:
				sym = K_F4;
				break;
			case SDLK_F5:
				sym = K_F5;
				break;
			case SDLK_F6:
				sym = K_F6;
				break;
			case SDLK_F7:
				sym = K_F7;
				break;
			case SDLK_F8:
				sym = K_F8;
				break;
			case SDLK_F9:
				sym = K_F9;
				break;
			case SDLK_F10:
				sym = K_F10;
				break;
			case SDLK_F11:
				sym = K_F11;
				break;
			case SDLK_F12:
				sym = K_F12;
				break;
			case SDLK_BREAK:
			case SDLK_PAUSE:
				sym = K_PAUSE;
				break;
			case SDLK_UP:
				sym = K_UPARROW;
				break;
			case SDLK_DOWN:
				sym = K_DOWNARROW;
				break;
			case SDLK_RIGHT:
				sym = K_RIGHTARROW;
				break;
			case SDLK_LEFT:
				sym = K_LEFTARROW;
				break;
			case SDLK_INSERT:
				sym = K_INS;
				break;
			case SDLK_HOME:
				sym = K_HOME;
				break;
			case SDLK_END:
				sym = K_END;
				break;
			case SDLK_PAGEUP:
				sym = K_PGUP;
				break;
			case SDLK_PAGEDOWN:
				sym = K_PGDN;
				break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
				sym = K_SHIFT;
				break;
			case SDLK_RCTRL:
			case SDLK_LCTRL:
				sym = K_CTRL;
				break;
			case SDLK_RALT:
			case SDLK_LALT:
				sym = K_ALT;
				break;
			case SDLK_RMETA:
			case SDLK_LMETA:
				sym = K_COMMAND;
				break;
			case SDLK_NUMLOCK:
				if (gamekey)
					sym = K_KP_NUMLOCK;
				else	sym = 0;
				break;
			case SDLK_KP0:
				if (gamekey)
					sym = K_KP_INS;
				else	sym = (modstate & KMOD_NUM) ? SDLK_0 : K_INS;
				break;
			case SDLK_KP1:
				if (gamekey)
					sym = K_KP_END;
				else	sym = (modstate & KMOD_NUM) ? SDLK_1 : K_END;
				break;
			case SDLK_KP2:
				if (gamekey)
					sym = K_KP_DOWNARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_2 : K_DOWNARROW;
				break;
			case SDLK_KP3:
				if (gamekey)
					sym = K_KP_PGDN;
				else	sym = (modstate & KMOD_NUM) ? SDLK_3 : K_PGDN;
				break;
			case SDLK_KP4:
				if (gamekey)
					sym = K_KP_LEFTARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_4 : K_LEFTARROW;
				break;
			case SDLK_KP5:
				if (gamekey)
					sym = K_KP_5;
				else	sym = SDLK_5;
				break;
			case SDLK_KP6:
				if (gamekey)
					sym = K_KP_RIGHTARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_6 : K_RIGHTARROW;
				break;
			case SDLK_KP7:
				if (gamekey)
					sym = K_KP_HOME;
				else	sym = (modstate & KMOD_NUM) ? SDLK_7 : K_HOME;
				break;
			case SDLK_KP8:
				if (gamekey)
					sym = K_KP_UPARROW;
				else	sym = (modstate & KMOD_NUM) ? SDLK_8 : K_UPARROW;
				break;
			case SDLK_KP9:
				if (gamekey)
					sym = K_KP_PGUP;
				else	sym = (modstate & KMOD_NUM) ? SDLK_9 : K_PGUP;
				break;
			case SDLK_KP_PERIOD:
				if (gamekey)
					sym = K_KP_DEL;
				else	sym = (modstate & KMOD_NUM) ? SDLK_PERIOD : K_DEL;
				break;
			case SDLK_KP_DIVIDE:
				if (gamekey)
					sym = K_KP_SLASH;
				else	sym = SDLK_SLASH;
				break;
			case SDLK_KP_MULTIPLY:
				if (gamekey)
					sym = K_KP_STAR;
				else	sym = SDLK_ASTERISK;
				break;
			case SDLK_KP_MINUS:
				if (gamekey)
					sym = K_KP_MINUS;
				else	sym = SDLK_MINUS;
				break;
			case SDLK_KP_PLUS:
				if (gamekey)
					sym = K_KP_PLUS;
				else	sym = SDLK_PLUS;
				break;
			case SDLK_KP_ENTER:
				if (gamekey)
					sym = K_KP_ENTER;
				else	sym = SDLK_RETURN;
				break;
			case SDLK_KP_EQUALS:
				if (gamekey)
					sym = 0;
				else	sym = SDLK_EQUALS;
				break;
			case 178: /* the '²' key */
				sym = '~';
				break;
			default:
			/* If we are not directly handled and still above 255,
			 * just force it to 0. kill unsupported international
			 * characters, too.  */
				if ((sym >= SDLK_WORLD_0 && sym <= SDLK_WORLD_95) ||
									sym > 255)
					sym = 0;
				break;
			}
			Key_Event (sym, state);
			if (event.type == SDL_KEYDOWN && !Key_ConsoleBindable(sym))
				Char_Event (sym);
			break;
#endif
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 1 ||
			    event.button.button > sizeof(buttonremap) / sizeof(buttonremap[0]))
			{
				Con_Printf ("Ignored event for mouse button %d\n",
							event.button.button);
				break;
			}
			Key_Event(buttonremap[event.button.button - 1], event.button.state == SDL_PRESSED);
			break;

#if defined(USE_SDL2)
		case SDL_MOUSEWHEEL:
			if (event.wheel.y > 0)
			{
				Key_Event(K_MWHEELUP, false);
				Key_Event(K_MWHEELUP, true);
			}
			else if (event.wheel.y < 0)
			{
				Key_Event(K_MWHEELDOWN, false);
				Key_Event(K_MWHEELDOWN, true);
			}
			break;
#endif

		case SDL_MOUSEMOTION:
			IN_MouseMove(event.motion.xrel, event.motion.yrel);
			break;

		case SDL_QUIT:
			CL_Disconnect ();
			Sys_Quit ();
			break;

		default:
			break;
		}
	}
}

