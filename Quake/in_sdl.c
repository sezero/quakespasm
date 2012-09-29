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
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif

static qboolean	prev_gamekey;

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

static int FilterMouseEvents (const SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return 0;
	}

	return 1;
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

	if (SDL_GetEventFilter() != NULL)
		SDL_SetEventFilter(NULL);

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
	}

	/* discard all mouse events when input is deactivated */
	if (SDL_GetEventFilter() != FilterMouseEvents)
		SDL_SetEventFilter(FilterMouseEvents);
}

void IN_Init (void)
{
	prev_gamekey = (key_dest == key_game || m_keys_bind_grab);
	SDL_EnableUNICODE (!prev_gamekey);
	if (SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL) == -1)
		Con_Printf("Warning: SDL_EnableKeyRepeat() failed.\n");

	if (safemode || COM_CheckParm("-nomouse"))
	{
		no_mouse = true;
		/* discard all mouse events when input is deactivated */
		SDL_SetEventFilter(FilterMouseEvents);
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

void IN_SendKeyEvents (void)
{
	SDL_Event event;
	int sym, usym, state, modstate;
	qboolean gamekey;

	gamekey = (key_dest == key_game || m_keys_bind_grab);
	if (gamekey != prev_gamekey)
	{
		prev_gamekey = gamekey;
		Key_ClearStates();
		SDL_EnableUNICODE(!gamekey);
	}

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_ACTIVEEVENT:
			if (event.active.state & (SDL_APPINPUTFOCUS|SDL_APPACTIVE))
			{
				if (event.active.gain)
					S_UnblockSound();
				else	S_BlockSound();
			}
			break;

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
			sym = event.key.keysym.sym;
			state = event.key.state;
			modstate = SDL_GetModState();

			switch (key_dest)
			{
			case key_game:
				if (event.key.keysym.unicode != 0)
				{	/* only use unicode for ~ and ` in game mode */
					if ((event.key.keysym.unicode & 0xFF80) == 0)
					{
						usym = event.key.keysym.unicode & 0x7F;
						if (usym == '`' || usym == '~')
							sym = usym;
					}
				}
				break;
			case key_message:
			case key_console:
				if (event.key.keysym.unicode != 0)
				{
#if defined(__APPLE__) && defined(__MACH__)
					if (sym == SDLK_BACKSPACE)
						break;	/* avoid change to SDLK_DELETE */
#endif	/* Mac OS X */
#if defined(__QNX__)
					if ((sym == SDLK_BACKSPACE) || (sym == SDLK_RETURN))
						break;	/* S.A: fixes QNX weirdness */
#endif	/* __QNX__ */
					if ((event.key.keysym.unicode & 0xFF80) == 0)
					{
						usym = event.key.keysym.unicode & 0x7F;
						if (modstate & KMOD_CTRL && usym < 32 && sym >= 32)
						{
							/* control characters */
							if (modstate & KMOD_SHIFT)
								usym += 64;
							else	usym += 96;
						}
						sym = usym;
					}
					/* else: it's an international character */
				}
			/*	printf("You pressed %s (%d) (%c)\n", SDL_GetKeyName(sym), sym, sym);*/
				break;
			default:
				break;
			}

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
			break;

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

