/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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

#include "arch_def.h"
#include "quakedef.h"

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#if defined(PLATFORM_OSX) || defined(PLATFORM_HAIKU)
#include <libgen.h>	/* dirname() and basename() */
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#ifdef DO_USERDIRS
#include <sys/stat.h>
#include <pwd.h>
#endif

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif
#else
#include "SDL.h"
#endif


qboolean		isDedicated;
cvar_t		sys_throttle = {"sys_throttle", "0.02", CVAR_ARCHIVE};

#define	MAX_HANDLES		32	/* johnfitz -- was 10 */
static FILE		*sys_handles[MAX_HANDLES];
static qboolean		stdinIsATTY;	/* from ioquake3 source */


static int findhandle (void)
{
	int i;

	for (i = 1; i < MAX_HANDLES; i++)
	{
		if (!sys_handles[i])
			return i;
	}
	Sys_Error ("out of handles");
	return -1;
}

long Sys_filelength (FILE *f)
{
	long		pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (const char *path, int *hndl)
{
	FILE	*f;
	int	i, retval;

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

int Sys_FileOpenWrite (const char *path)
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

int Sys_FileWrite (int handle, const void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int Sys_FileType (const char *path)
{
	/*
	if (access(path, R_OK) == -1)
		return 0;
	*/
	struct stat	st;

	if (stat(path, &st) != 0)
		return FS_ENT_NONE;
	if (S_ISDIR(st.st_mode))
		return FS_ENT_DIRECTORY;
	if (S_ISREG(st.st_mode))
		return FS_ENT_FILE;

	return FS_ENT_NONE;
}


#if defined(__linux__) || defined(__sun) || defined(sun) || defined(_AIX)
static int Sys_NumCPUs (void)
{
	int numcpus = sysconf(_SC_NPROCESSORS_ONLN);
	return (numcpus < 1) ? 1 : numcpus;
}

#elif defined(PLATFORM_OSX)
#include <sys/sysctl.h>
#if !defined(HW_AVAILCPU)	/* using an ancient SDK? */
#define HW_AVAILCPU		25	/* needs >= 10.2 */
#endif
static int Sys_NumCPUs (void)
{
	int numcpus;
	int mib[2];
	size_t len;

#if defined(_SC_NPROCESSORS_ONLN)	/* needs >= 10.5 */
	numcpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (numcpus != -1)
		return (numcpus < 1) ? 1 : numcpus;
#endif
	len = sizeof(numcpus);
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;
	sysctl(mib, 2, &numcpus, &len, NULL, 0);
	if (sysctl(mib, 2, &numcpus, &len, NULL, 0) == -1)
	{
		mib[1] = HW_NCPU;
		if (sysctl(mib, 2, &numcpus, &len, NULL, 0) == -1)
			return 1;
	}
	return (numcpus < 1) ? 1 : numcpus;
}

#elif defined(__sgi) || defined(sgi) || defined(__sgi__) /* IRIX */
static int Sys_NumCPUs (void)
{
	int numcpus = sysconf(_SC_NPROC_ONLN);
	if (numcpus < 1)
		numcpus = 1;
	return numcpus;
}

#elif defined(PLATFORM_BSD)
#include <sys/sysctl.h>
static int Sys_NumCPUs (void)
{
	int numcpus;
	int mib[2];
	size_t len;

#if defined(_SC_NPROCESSORS_ONLN)
	numcpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (numcpus != -1)
		return (numcpus < 1) ? 1 : numcpus;
#endif
	len = sizeof(numcpus);
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &numcpus, &len, NULL, 0) == -1)
		return 1;
	return (numcpus < 1) ? 1 : numcpus;
}

#elif defined(__hpux) || defined(__hpux__) || defined(_hpux)
#include <sys/mpctl.h>
static int Sys_NumCPUs (void)
{
	int numcpus = mpctl(MPC_GETNUMSPUS, NULL, NULL);
	return numcpus;
}

#else /* unknown OS */
static int Sys_NumCPUs (void)
{
	return -2;
}
#endif

static char basedir[MAX_OSPATH];
static char userdir[MAX_OSPATH];
#ifdef DO_USERDIRS
#ifdef PLATFORM_HAIKU
#define SYS_USERDIR "QuakeSpasm"
#include <FindDirectory.h>
#include <fs_info.h>
static void Sys_GetUserdir (char *dst, size_t dstsize)
{
	dev_t volume = dev_for_path("/boot");
	char buffer[B_PATH_NAME_LENGTH];
	status_t result;

	result = find_directory(B_USER_SETTINGS_DIRECTORY, volume, false, buffer, sizeof(buffer));
	if (result != B_OK)
		Sys_Error ("Couldn't determine userspace directory");

	q_snprintf (dst, dstsize, "%s/%s", buffer, SYS_USERDIR);
}
static void Sys_GetBasedir (char *dst, size_t dstsize)
{
	dev_t volume = dev_for_path("/boot");
	char buffer[B_PATH_NAME_LENGTH];
	status_t result;

	result = find_directory(B_USER_NONPACKAGED_DATA_DIRECTORY, volume, false, buffer, sizeof(buffer));
	if (result != B_OK)
		Sys_Error ("Couldn't determine userspace directory");

	q_snprintf (dst, dstsize, "%s/%s", buffer, SYS_USERDIR);
}
#else /* PLATFORM_HAIKU */
#define SYS_USERDIR "quakespasm"
static void Sys_GetUserdir (char *dst, size_t dstsize)
{
#ifdef USE_SDL2
	char *prefpath = SDL_GetPrefPath (NULL, SYS_USERDIR);
	if (prefpath == NULL)
		Sys_Error ("Couldn't determine userspace directory");
	q_snprintf (dst, dstsize, "%s", prefpath);
	SDL_free (prefpath);
#else /* USE_SDL2 */
	/* note - we're specifically not using XDG_CONFIG_HOME here so we can match
	 * the behaviour of SDL_GetPrefPath()
	 */
	const char *env = getenv("XDG_DATA_HOME");
	if (env)
	{
		q_snprintf (dst, dstsize, "%s/%s", env, SYS_USERDIR);
	}
	else
	{
		/* $XDG_DATA_HOME failed, try getpwuid */
		struct passwd *pw = getpwuid(getuid());
		/* give up */
		if (!pw)
			Sys_Error ("Couldn't determine userspace directory");
		env = pw->pw_dir;
		q_snprintf (dst, dstsize, "%s/.local/share/%s", env, SYS_USERDIR);
	}
#endif /* USE_SDL2 */
}
static void Sys_GetBasedir (char *dst, size_t dstsize)
{
	const char *env = getenv("XDG_DATA_HOME");
	if (env)
	{
		/* user-defined quake dir */
		q_snprintf (dst, dstsize, "%s/games/quake", env);
	}
	else
	{
		/* fall back to system quake dir */
		q_snprintf (dst, dstsize, "%s", "/usr/local/share/games/quake");
	}
}
#endif /* PLATFORM_HAIKU */
#else /* DO_USERDIRS */
static void Sys_GetBasedir (char *dst, size_t dstsize)
{
#ifdef USE_SDL2
	char *basepath = SDL_GetBasePath ();
	if (basepath == NULL)
		Sys_Error ("Couldn't determine base directory");
	q_snprintf (dst, dstsize, "%s", basepath);
	SDL_free (basepath);
#else /* USE_SDL2 */
	if (getcwd(dst, dstsize - 1) == NULL)
		Sys_Error ("Couldn't determine base directory");
#endif /* USE_SDL2 */
}
#endif /* DO_USERDIRS */

void Sys_Init (void)
{
	const char* term = getenv("TERM");
	stdinIsATTY = isatty(STDIN_FILENO) &&
			!(term && (!strcmp(term, "raw") || !strcmp(term, "dumb")));
	if (!stdinIsATTY)
		Sys_Printf("Terminal input not available.\n");

	memset (basedir, 0, sizeof(basedir));
	memset (userdir, 0, sizeof(userdir));

	Sys_GetBasedir (basedir, sizeof(basedir));

#ifdef DO_USERDIRS
	Sys_GetUserdir (userdir, sizeof(userdir));
#else
	Sys_GetBasedir (userdir, sizeof(userdir));
#endif

	host_parms->basedir = basedir;
	host_parms->userdir = userdir;
	Sys_mkdir (host_parms->basedir);
	Sys_mkdir (host_parms->userdir);

	Sys_Printf("Basedir: %s\nUserdir: %s\n", host_parms->basedir, host_parms->userdir);
	host_parms->numcpus = Sys_NumCPUs ();
	Sys_Printf("Detected %d CPUs.\n", host_parms->numcpus);
}

void Sys_mkdir (const char *path)
{
	int rc = mkdir (path, 0777);
	if (rc != 0 && errno == EEXIST)
	{
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			rc = 0;
	}
	if (rc != 0)
	{
		rc = errno;
		Sys_Error("Unable to create directory %s: %s", path, strerror(rc));
	}
}

static const char errortxt1[] = "\nERROR-OUT BEGIN\n\n";
static const char errortxt2[] = "\nQUAKE ERROR: ";

void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	host_parms->errstate++;

	va_start (argptr, error);
	q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	fputs (errortxt1, stderr);
	Host_Shutdown ();
	fputs (errortxt2, stderr);
	fputs (text, stderr);
	fputs ("\n\n", stderr);
	if (!isDedicated)
		PL_ErrorDialog(text);

	exit (1);
}

void Sys_Printf (const char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

void Sys_Quit (void)
{
	Host_Shutdown();

	exit (0);
}

double Sys_DoubleTime (void)
{
	return SDL_GetTicks() / 1000.0;
}

const char *Sys_ConsoleInput (void)
{
	static qboolean	con_eof = false;
	static char	con_text[256];
	static int	textlen;
	char		c;
	fd_set		set;
	struct timeval	timeout;

	if (!stdinIsATTY || con_eof)
		return NULL;

	FD_ZERO (&set);
	FD_SET (0, &set);	// stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (select (1, &set, NULL, NULL, &timeout))
	{
		if (read(0, &c, 1) <= 0)
		{
			// Finish processing whatever is already in the
			// buffer (if anything), then stop reading
			con_eof = true;
			c = '\n';
		}
		if (c == '\n' || c == '\r')
		{
			con_text[textlen] = '\0';
			textlen = 0;
			return con_text;
		}
		else if (c == 8)
		{
			if (textlen)
			{
				textlen--;
				con_text[textlen] = '\0';
			}
			continue;
		}
		con_text[textlen] = c;
		textlen++;
		if (textlen < (int) sizeof(con_text))
			con_text[textlen] = '\0';
		else
		{
		// buffer is full
			textlen = 0;
			con_text[0] = '\0';
			Sys_Printf("\nConsole input too long!\n");
			break;
		}
	}

	return NULL;
}

void Sys_Sleep (unsigned long msecs)
{
/*	usleep (msecs * 1000);*/
	SDL_Delay (msecs);
}

void Sys_SendKeyEvents (void)
{
	IN_Commands();		//ericw -- allow joysticks to add keys so they can be used to confirm SCR_ModalMessage
	IN_SendKeyEvents();
}

