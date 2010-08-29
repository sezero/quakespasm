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
#include "SDL.h"

static qboolean	no_mouse = false;

// mouse variables
cvar_t	m_filter = {"m_filter","0"};

// total accumulated mouse movement since last frame
// this gets updated from the main game loop via IN_MouseMove
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

void IN_Activate (void)
{
	if (no_mouse)
		return;

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

	// discard all mouse events when input is deactivated
	if (SDL_GetEventFilter() != FilterMouseEvents)
		SDL_SetEventFilter(FilterMouseEvents);
}

void IN_Init (void)
{
	BuildKeyMaps();

	if (SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL) == -1)
		Con_Printf("Warning: SDL_EnableKeyRepeat() failed.\n");

	if (safemode || COM_CheckParm("-nomouse"))
	{
		no_mouse = true;
		// discard all mouse events when input is deactivated
		SDL_SetEventFilter(FilterMouseEvents);
	}

	IN_Activate();
}

void IN_Shutdown (void)
{
	IN_Deactivate(true);
}

void IN_Commands (void)
{
// TODO: implement this for joystick support
}

extern cvar_t cl_maxpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; //johnfitz -- variable pitch clamping


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
		V_StopPitchDrift ();

	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * dmy;
		//johnfitz -- variable pitch clamping
		if (cl.viewangles[PITCH] > cl_maxpitch.value)
			cl.viewangles[PITCH] = cl_maxpitch.value;
		if (cl.viewangles[PITCH] < cl_minpitch.value)
			cl.viewangles[PITCH] = cl_minpitch.value;
		//johnfitz
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


/* =================================
   SDL to quake keynum mapping
   =================================*/

static byte key_map[SDLK_LAST];

void BuildKeyMaps (void)
{
	int i;

	for (i = 0; i < SDLK_LAST; i++)
		key_map[i] = 0;

	key_map[SDLK_BACKSPACE]     = K_BACKSPACE;
	key_map[SDLK_TAB]           = K_TAB;
	key_map[SDLK_RETURN]        = K_ENTER;
	key_map[SDLK_PAUSE]         = K_PAUSE;
	key_map[SDLK_ESCAPE]        = K_ESCAPE;
	key_map[SDLK_SPACE]         = K_SPACE;
	key_map[SDLK_EXCLAIM]       = '!';
	key_map[SDLK_QUOTEDBL]      = '"';
	key_map[SDLK_HASH]          = '#';
	key_map[SDLK_DOLLAR]        = '$';
	key_map[SDLK_AMPERSAND]     = '&';
	key_map[SDLK_QUOTE]         = '\'';
	key_map[SDLK_LEFTPAREN]     = '(';
	key_map[SDLK_RIGHTPAREN]    = ')';
	key_map[SDLK_ASTERISK]      = '*';
	key_map[SDLK_PLUS]          = '+';
	key_map[SDLK_COMMA]         = ',';
	key_map[SDLK_MINUS]         = '-';
	key_map[SDLK_PERIOD]        = '.';
	key_map[SDLK_SLASH]         = '/';

	key_map[SDLK_0]             = '0';
	key_map[SDLK_1]             = '1';
	key_map[SDLK_2]             = '2';
	key_map[SDLK_3]             = '3';
	key_map[SDLK_4]             = '4';
	key_map[SDLK_5]             = '5';
	key_map[SDLK_6]             = '6';
	key_map[SDLK_7]             = '7';
	key_map[SDLK_8]             = '8';
	key_map[SDLK_9]             = '9';

	key_map[SDLK_COLON]         = ':';
	key_map[SDLK_SEMICOLON]     = ';';
	key_map[SDLK_LESS]          = '<';
	key_map[SDLK_EQUALS]        = '=';
	key_map[SDLK_GREATER]       = '>';
	key_map[SDLK_QUESTION]      = '?';
	key_map[SDLK_AT]            = '@';
	key_map[SDLK_LEFTBRACKET]   = '[';
	key_map[SDLK_BACKSLASH]     = '\\';
	key_map[SDLK_RIGHTBRACKET]  = ']';
	key_map[SDLK_CARET]         = '^';
	key_map[SDLK_UNDERSCORE]    = '_';
	key_map[SDLK_BACKQUOTE]     = '`';

	key_map[SDLK_a]             = 'a';
	key_map[SDLK_b]             = 'b';
	key_map[SDLK_c]             = 'c';
	key_map[SDLK_d]             = 'd';
	key_map[SDLK_e]             = 'e';
	key_map[SDLK_f]             = 'f';
	key_map[SDLK_g]             = 'g';
	key_map[SDLK_h]             = 'h';
	key_map[SDLK_i]             = 'i';
	key_map[SDLK_j]             = 'j';
	key_map[SDLK_k]             = 'k';
	key_map[SDLK_l]             = 'l';
	key_map[SDLK_m]             = 'm';
	key_map[SDLK_n]             = 'n';
	key_map[SDLK_o]             = 'o';
	key_map[SDLK_p]             = 'p';
	key_map[SDLK_q]             = 'q';
	key_map[SDLK_r]             = 'r';
	key_map[SDLK_s]             = 's';
	key_map[SDLK_t]             = 't';
	key_map[SDLK_u]             = 'u';
	key_map[SDLK_v]             = 'v';
	key_map[SDLK_w]             = 'w';
	key_map[SDLK_x]             = 'x';
	key_map[SDLK_y]             = 'y';
	key_map[SDLK_z]             = 'z';

	key_map[SDLK_DELETE]        = K_DEL;

	key_map[SDLK_KP0]           = KP_INS;
	key_map[SDLK_KP1]           = KP_END;
	key_map[SDLK_KP2]           = KP_DOWNARROW;
	key_map[SDLK_KP3]           = KP_PGDN;
	key_map[SDLK_KP4]           = KP_LEFTARROW;
	key_map[SDLK_KP5]           = KP_5;
	key_map[SDLK_KP6]           = KP_RIGHTARROW;
	key_map[SDLK_KP7]           = KP_HOME;
	key_map[SDLK_KP8]           = KP_UPARROW;
	key_map[SDLK_KP9]           = KP_PGUP;
	key_map[SDLK_KP_PERIOD]     = KP_DEL;
	key_map[SDLK_KP_DIVIDE]     = KP_SLASH;
	key_map[SDLK_KP_MULTIPLY]   = KP_STAR;
	key_map[SDLK_KP_MINUS]      = KP_MINUS;
	key_map[SDLK_KP_PLUS]       = KP_PLUS;
	key_map[SDLK_KP_ENTER]      = KP_ENTER;
	key_map[SDLK_KP_EQUALS]     = 0;

	key_map[SDLK_UP]            = K_UPARROW;
	key_map[SDLK_DOWN]          = K_DOWNARROW;
	key_map[SDLK_RIGHT]         = K_RIGHTARROW;
	key_map[SDLK_LEFT]          = K_LEFTARROW;
	key_map[SDLK_INSERT]        = K_INS;
	key_map[SDLK_HOME]          = K_HOME;
	key_map[SDLK_END]           = K_END;
	key_map[SDLK_PAGEUP]        = K_PGUP;
	key_map[SDLK_PAGEDOWN]      = K_PGDN;

	key_map[SDLK_F1]            = K_F1;
	key_map[SDLK_F2]            = K_F2;
	key_map[SDLK_F3]            = K_F3;
	key_map[SDLK_F4]            = K_F4;
	key_map[SDLK_F5]            = K_F5;
	key_map[SDLK_F6]            = K_F6;
	key_map[SDLK_F7]            = K_F7;
	key_map[SDLK_F8]            = K_F8;
	key_map[SDLK_F9]            = K_F9;
	key_map[SDLK_F10]           = K_F10;
	key_map[SDLK_F11]           = K_F11;
	key_map[SDLK_F12]           = K_F12;
	key_map[SDLK_F13]           = 0;
	key_map[SDLK_F14]           = 0;
	key_map[SDLK_F15]           = 0;

	key_map[SDLK_NUMLOCK]       = KP_NUMLOCK;
	key_map[SDLK_CAPSLOCK]      = 0;
	key_map[SDLK_SCROLLOCK]     = 0;
	key_map[SDLK_RSHIFT]        = K_SHIFT;
	key_map[SDLK_LSHIFT]        = K_SHIFT;
	key_map[SDLK_RCTRL]         = K_CTRL;
	key_map[SDLK_LCTRL]         = K_CTRL;
	key_map[SDLK_RALT]          = K_ALT;
	key_map[SDLK_LALT]          = K_ALT;
	key_map[SDLK_RMETA]         = 0;
	key_map[SDLK_LMETA]         = 0;
	key_map[SDLK_LSUPER]        = 0;
	key_map[SDLK_RSUPER]        = 0;
	key_map[SDLK_MODE]          = 0;
	key_map[SDLK_COMPOSE]       = 0;
	key_map[SDLK_HELP]          = 0;
	key_map[SDLK_PRINT]         = 0;
	key_map[SDLK_SYSREQ]        = 0;
	key_map[SDLK_BREAK]         = 0;
	key_map[SDLK_MENU]          = 0;
	key_map[SDLK_POWER]         = 0;
	key_map[SDLK_EURO]          = 0;
	key_map[SDLK_UNDO]          = 0;
}

/*
=======
MapKey

Map from SDL to quake keynums
=======
*/
int Key_Map (void *event) /* SDL_KeyboardEvent *event */
{
	return key_map[(*(SDL_KeyboardEvent *)event).keysym.sym];

	/* TODO: keypad handling
	if (cl_keypad.value) {
		if (extended) {
			switch (key) {
			case K_ENTER:		return KP_ENTER;
			case '/':		return KP_SLASH;
			case K_PAUSE:		return KP_NUMLOCK;
			};
		} else {
			switch (key) {
			case K_HOME:		return KP_HOME;
			case K_UPARROW:		return KP_UPARROW;
			case K_PGUP:		return KP_PGUP;
			case K_LEFTARROW:	return KP_LEFTARROW;
			case K_RIGHTARROW:	return KP_RIGHTARROW;
			case K_END:			return KP_END;
			case K_DOWNARROW:	return KP_DOWNARROW;
			case K_PGDN:		return KP_PGDN;
			case K_INS:			return KP_INS;
			case K_DEL:			return KP_DEL;
			}
		}
	} else {
		// cl_keypad 0, compatibility mode
		switch (key) {
			case KP_STAR:	return '*';
			case KP_MINUS:	return '-';
			case KP_5:		return '5';
			case KP_PLUS:	return '+';
		}
	}
	*/
}

