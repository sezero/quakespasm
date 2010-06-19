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
#include <windows.h>
#include "SDL.h"
#include "SDL_syswm.h"

HICON icon;

void PL_SetWindowIcon (void)
{
    HINSTANCE handle;
    SDL_SysWMinfo wminfo;
    HWND hwnd;

    handle = GetModuleHandle(NULL);
    icon = LoadIcon(handle, "icon");

    if (!icon)
        return; // no icon in executable

    SDL_VERSION(&wminfo.version);

    if (SDL_GetWMInfo(&wminfo) != 1)
        return; // wrong SDL version

    hwnd = wminfo.window;
#ifdef _WIN64
    SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR) icon);
#else
    SetClassLong(hwnd, GCL_HICON, (LONG) icon);
#endif
}

void PL_VID_Shutdown (void)
{
    DestroyIcon(icon);
}

void PL_ErrorDialog(const char *text)
{
    MessageBox(NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
}

