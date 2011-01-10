/*
 * Background music handling for Quakespasm
 * Handle cases when we are configured for no sound and no midi driver,
 * nada...
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2010 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "quakedef.h"
#include "bgmusic.h"

qboolean	bgmloop = true;
cvar_t		bgm_extmusic = {"bgm_extmusic", "1", true};

static float	old_volume = -1.0f;

qboolean BGM_Init (void)
{
	Cvar_RegisterVariable(&bgm_extmusic, NULL);
	return false;
}

void BGM_Play (const char *filename)
{
}

void BGM_PlayCDtrack (byte track, qboolean looping)
{
	bgmloop = looping;
	CDAudio_Play(track, looping);
}

void BGM_Stop (void)
{
}

void BGM_Pause (void)
{
}

void BGM_Resume (void)
{
}

void BGM_Update (void)
{
	if (old_volume != bgmvolume.value)
	{
		if (bgmvolume.value < 0)
			Cvar_Set ("bgmvolume", "0");
		else if (bgmvolume.value > 1)
			Cvar_Set ("bgmvolume", "1");
		old_volume = bgmvolume.value;
	}
}

