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

static const Uint8 bmp_bytes[] =
{
#include "qs_bmp.h"
};

void PL_SetWindowIcon (void)
{
	SDL_RWops	*rwop;
	SDL_Surface	*icon;

	/* SDL_RWFromConstMem() requires SDL >= 1.2.7 */
	rwop = SDL_RWFromConstMem(bmp_bytes, sizeof(bmp_bytes));
	if (rwop == NULL)
		return;
	icon = SDL_LoadBMP_RW(rwop, 1);
	if (icon == NULL)
		return;
	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}

void PL_VID_Shutdown (void)
{
}

void PL_ErrorDialog (const char *errorMsg)
{
// TODO: we can dlopen gtk for an error
// dialog window. would it be worth it?
}

