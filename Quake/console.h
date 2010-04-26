/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

#ifndef __CONSOLE_H
#define __CONSOLE_H

//
// console
//
extern int con_totallines;
extern int con_backscroll;
extern	qboolean con_forcedup;	// because no entities to refresh
extern qboolean con_initialized;
extern byte *con_chars;

extern char con_lastcenterstring[]; //johnfitz

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, qboolean drawinput);
void Con_Print (const char *txt);
void Con_Printf (const char *fmt, ...) __attribute__((__format__(__printf__,1,2)));
void Con_Warning (const char *fmt, ...) __attribute__((__format__(__printf__,1,2))); //johnfitz
void Con_DPrintf (const char *fmt, ...) __attribute__((__format__(__printf__,1,2)));
void Con_DPrintf2 (const char *fmt, ...) __attribute__((__format__(__printf__,1,2))); //johnfitz
void Con_SafePrintf (const char *fmt, ...) __attribute__((__format__(__printf__,1,2)));
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

void Con_NotifyBox (const char *text);	// during startup for sound / cd warnings

void Con_Show (void);
void Con_Hide (void);

char *Con_Quakebar (int len);
void Con_TabComplete (void);
void Con_LogCenterPrint (const char *str);

#endif	/* __CONSOLE_H */

