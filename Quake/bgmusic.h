/*
 * Background music handling for Hexen II: Hammer of Thyrion (uHexen2)
 * Handles streaming music as raw sound samples and runs the midi driver
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2010 O.Sezer <sezero@users.sourceforge.net>
 *
 * $Id: bgmusic.h 3818 2010-12-19 09:04:17Z sezero $
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

#ifndef _BGMUSIC_H_
#define _BGMUSIC_H_

extern qboolean	bgmloop;

qboolean BGM_Init (void);
void BGM_Shutdown (void);

void BGM_Play (const char *filename);
void BGM_PlayCDtrack (byte track, qboolean looping);
void BGM_Stop (void);
void BGM_Update (void);
void BGM_Pause (void);
void BGM_Resume (void);

#endif	/* _BGMUSIC_H_ */

