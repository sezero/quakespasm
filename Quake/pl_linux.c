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

static Uint8 *PL_CreateIconMask(SDL_Surface *icon)
{
	int	x, y;
	int	i;
	Uint8	*topleft_pixel, *current_pixel;
	Uint8	*mask;

	/* Note that this sets all pixels to invisible in the mask. */
	mask = (Uint8 *) calloc(((icon->w + 7) / 8) * icon->h, sizeof(Uint8));
	if (!mask)
		return NULL;

	if (SDL_MUSTLOCK(icon))
		SDL_LockSurface(icon);

	/* The top-left pixel and identical pixels will remain invisible. */
	topleft_pixel = (Uint8 *)icon->pixels;
	for (x = 0; x < icon->w; x++)
	{
		for (y = 0; y < icon->h; y++)
		{
			current_pixel = (Uint8 *)icon->pixels +
					y * icon->pitch +
					x * icon->format->BytesPerPixel;
			for (i = 0; i < icon->format->BytesPerPixel; i++)
			{
				/* If the current pixel is not identical to
				 * the top-left pixel, set it to visible in
				 * the mask. */
				if (current_pixel[i] != topleft_pixel[i])
				{
					mask[y * ((icon->w + 7) / 8) + x / 8] |= 128 >> x % 8;
					break;
				}
			}
		}
	}

	if (SDL_MUSTLOCK(icon))
		SDL_UnlockSurface(icon);

	return mask;
}

void PL_SetWindowIcon (void)
{
	SDL_RWops	*rwop;
	SDL_Surface	*icon;
	Uint8		*mask;

	/* SDL_RWFromConstMem() requires SDL >= 1.2.7 */
	rwop = SDL_RWFromConstMem(bmp_bytes, sizeof(bmp_bytes));
	if (rwop == NULL)
		return;
	icon = SDL_LoadBMP_RW(rwop, 1);
	if (icon == NULL)
		return;
	mask = PL_CreateIconMask(icon);
	if (mask != NULL)
	{
		SDL_WM_SetIcon(icon, mask);
		free(mask);
	}
	SDL_FreeSurface(icon);
}

void PL_VID_Shutdown (void)
{
}

void PL_ErrorDialog (const char *errorMsg)
{
}

