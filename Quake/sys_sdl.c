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

#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include "errno.h"

#include "quakedef.h"

#define CONSOLE_ERROR_TIMEOUT	60.0	/* # of seconds to wait on Sys_Error running */
qboolean		isDedicated;
static qboolean		sc_return_on_enter = false;

#define	MAX_HANDLES		32	/* johnfitz -- was 10 */
FILE			*sys_handles[MAX_HANDLES];

int findhandle (void)
{
	int i;

	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;

	Sys_Error ("out of handles");
	return -1;
}

int Sys_filelength (FILE *f)
{
	int pos;
	int end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE	*f;
	int		i, retval;

	i = findhandle ();
	f = fopen(path, "rb");

	if (!f)
	{
		*hndl = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = Sys_filelength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (char *path)
{
	FILE	*f;
	int		i;

	i = findhandle ();
	f = fopen(path, "wb");

	if (!f)
		Sys_Error ("Error opening %s: %s", path, strerror(errno));

	sys_handles[i] = f;
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int Sys_FileTime (char *path)
{
	FILE	*f;

	f = fopen(path, "rb");

	if (f)
	{
		fclose(f);
		return 1;
	}

	return -1;
}

void Sys_mkdir (char *path)
{
#ifdef _WIN32
	int rc = _mkdir (path);
#else
	int rc = mkdir (path, 0777);
#endif
	if (rc != 0 && errno != EEXIST)
		Sys_Error("Unable to create directory %s", path);
}


void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024], text2[1024];
	const char	text3[] = "Press Enter to exit\n";
	const char	text4[] = "***********************************\n";
	const char	text5[] = "\n";
	double		starttime;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;
	static int	in_sys_error3 = 0;

	if (!in_sys_error3)
	{
		in_sys_error3 = 1;
	}

	Host_Shutdown ();

	//TODO: use OS messagebox here if possible
	// (windows, os x and linux shouldn't be a problem)
	//implement this in pl_*, which contains all the
	// platform dependent code

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	if (isDedicated)
	{
		sprintf (text2, "ERROR: %s\n", text);
		printf ("%s", text5);
		printf ("%s", text4);
		printf ("%s", text2);
		printf ("%s", text3);
		printf ("%s", text4);

		starttime = Sys_FloatTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here

		while (!Sys_ConsoleInput () &&
			((Sys_FloatTime () - starttime) < CONSOLE_ERROR_TIMEOUT))
		{
		}

		if (!in_sys_error1)
		{
			in_sys_error1 = 1;
			Host_Shutdown ();
		}
	}
	else
	{
		PL_ErrorDialog(text);
	}

// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
	//	DeinitConProc ();
	}

	exit (1);
}

void Sys_Printf (const char *fmt, ...)
{
	va_list argptr;

// always print to the console
//	if (isDedicated)
//	{
		va_start(argptr, fmt);
		vprintf(fmt, argptr);
		va_end(argptr);
//	}
}

void Sys_Quit (void)
{
	Host_Shutdown();

//	if (isDedicated)
//		FreeConsole ();

// shut down QHOST hooks if necessary
//	DeinitConProc ();

	exit (0);
}

double Sys_FloatTime (void)
{
	return SDL_GetTicks() / 1000.0;
}

char *Sys_ConsoleInput (void)
{
	return 0;
}

void Sys_Sleep (void)
{
}

void Sys_SendKeyEvents (void)
{
	SDL_Event event;

	SDL_PumpEvents();
	while (SDL_PollEvent (&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			Key_Event(Key_Map(&(event.key)), event.key.type == SDL_KEYDOWN);
			return;
		case SDL_QUIT:
			Sys_Quit();
			break;
		default:
			SDL_PumpEvents();
			break;
		}
	}
}

void Sys_LowFPPrecision (void)
{
}

void Sys_HighFPPrecision (void)
{
}

void Sys_SetFPCW (void)
{
}

