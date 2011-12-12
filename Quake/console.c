/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

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
// console.c

#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "quakedef.h"

int 		con_linewidth;

float		con_cursorspeed = 4;

#define		CON_TEXTSIZE 65536 //johnfitz -- new default size
#define		CON_MINSIZE  16384 //johnfitz -- old default, now the minimum size
int			con_buffersize; //johnfitz -- user can now override default

qboolean 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text = NULL;

cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_logcenterprint = {"con_logcenterprint", "1"}; //johnfitz

char		con_lastcenterstring[1024]; //johnfitz

#define	NUM_CON_TIMES 4
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
										// for transparent notify lines

int			con_vislines;

qboolean	con_debuglog = false;

extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;

qboolean	con_initialized;

extern void M_Menu_Main_f (void);

/*
================
Con_Quakebar -- johnfitz -- returns a bar of the desired length, but never wider than the console

includes a newline, unless len >= con_linewidth.
================
*/
const char *Con_Quakebar (int len)
{
	static char bar[42];
	int i;

	len = q_min(len, sizeof(bar) - 2);
	len = q_min(len, con_linewidth);

	bar[0] = '\35';
	for (i = 1; i < len - 1; i++)
		bar[i] = '\36';
	bar[len-1] = '\37';

	if (len < con_linewidth)
	{
		bar[len] = '\n';
		bar[len+1] = 0;
	}
	else
		bar[len] = 0;

	return bar;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	extern int history_line; //johnfitz

	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			IN_Activate();
			key_dest = key_game;
			key_lines[edit_line][1] = 0;	// clear any typing
			key_linepos = 1;
			con_backscroll = 0; //johnfitz -- toggleconsole should return you to the bottom of the scrollback
			history_line = edit_line; //johnfitz -- it should also return you to the bottom of the command history
		}
		else
		{
			M_Menu_Main_f ();
		}
	}
	else
	{
		IN_Deactivate(vid.type == MODE_WINDOWED);
		key_dest = key_console;
	}

	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		Q_memset (con_text, ' ', con_buffersize); //johnfitz -- con_buffersize replaces CON_TEXTSIZE
	con_backscroll = 0; //johnfitz -- if console is empty, being scrolled up is confusing
}

/*
================
Con_Dump_f -- johnfitz -- adapted from quake2 source
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	const char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

#if 1
	//johnfitz -- there is a security risk in writing files with an arbitrary filename. so,
	//until stuffcmd is crippled to alleviate this risk, just force the default filename.
	sprintf (name, "%s/condump.txt", com_gamedir);
#else
	if (Cmd_Argc() > 2)
	{
		Con_Printf ("usage: condump <filename>\n");
		return;
	}

	if (Cmd_Argc() > 1)
	{
		if (strstr(Cmd_Argv(1), ".."))
		{
			Con_Printf ("Relative pathnames are not allowed.\n");
			return;
		}
		sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
		COM_DefaultExtension (name, ".txt");
	}
	else
		sprintf (name, "%s/condump.txt", com_gamedir);
#endif

	COM_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open file %s.\n", name);
		return;
	}

	// skip initial empty lines
	for (l = con_current - con_totallines + 1 ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		for (x=0 ; x<con_linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con_linewidth)
			break;
	}

	// write the remaining lines
	buffer[con_linewidth] = 0;
	for ( ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		strncpy (buffer, line, con_linewidth);
		for (x=con_linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
	Con_Printf ("Dumped console text to %s.\n", name);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;

	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}


/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	key_dest = key_message;
	team_message = false;
}


/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	team_message = true;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	*tbuf; //johnfitz -- tbuf no longer a static array
	int mark; //johnfitz

	width = (vid.conwidth >> 3) - 2; //johnfitz -- use vid.conwidth instead of vid.width

	if (width == con_linewidth)
		return;

	oldwidth = con_linewidth;
	con_linewidth = width;
	oldtotallines = con_totallines;
	con_totallines = con_buffersize / con_linewidth; //johnfitz -- con_buffersize replaces CON_TEXTSIZE
	numlines = oldtotallines;

	if (con_totallines < numlines)
		numlines = con_totallines;

	numchars = oldwidth;

	if (con_linewidth < numchars)
		numchars = con_linewidth;

	mark = Hunk_LowMark (); //johnfitz
	tbuf = (char *) Hunk_Alloc (con_buffersize); //johnfitz

	Q_memcpy (tbuf, con_text, con_buffersize);//johnfitz -- con_buffersize replaces CON_TEXTSIZE
	Q_memset (con_text, ' ', con_buffersize);//johnfitz -- con_buffersize replaces CON_TEXTSIZE

	for (i=0 ; i<numlines ; i++)
	{
		for (j=0 ; j<numchars ; j++)
		{
			con_text[(con_totallines - 1 - i) * con_linewidth + j] =
					tbuf[((con_current - i + oldtotallines) % oldtotallines) * oldwidth + j];
		}
	}

	Hunk_FreeToLowMark (mark); //johnfitz

	Con_ClearNotify ();

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	int i;

	//johnfitz -- user settable console buffer size
	i = COM_CheckParm("-consize");
	if (i && i < com_argc-1)
		con_buffersize = q_max(CON_MINSIZE,Q_atoi(com_argv[i+1])*1024);
	else
		con_buffersize = CON_TEXTSIZE;
	//johnfitz

	con_text = (char *) Hunk_AllocName (con_buffersize, "context");//johnfitz -- con_buffersize replaces CON_TEXTSIZE
	Q_memset (con_text, ' ', con_buffersize);//johnfitz -- con_buffersize replaces CON_TEXTSIZE
	con_linewidth = -1;

	//johnfitz -- no need to run Con_CheckResize here
	con_linewidth = 38;
	con_totallines = con_buffersize / con_linewidth;//johnfitz -- con_buffersize replaces CON_TEXTSIZE
	con_backscroll = 0;
	con_current = con_totallines - 1;
	//johnfitz

	Con_Printf ("Console initialized.\n");

//
// register our commands
//
	Cvar_RegisterVariable (&con_notifytime, NULL);
	Cvar_RegisterVariable (&con_logcenterprint, NULL); //johnfitz

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f); //johnfitz
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	//johnfitz -- improved scrolling
	if (con_backscroll)
		con_backscroll++;
	if (con_backscroll > con_totallines - (glheight>>3) - 1)
		con_backscroll = con_totallines - (glheight>>3) - 1;
	//johnfitz

	con_x = 0;
	con_current++;
	Q_memset (&con_text[(con_current%con_totallines)*con_linewidth]
	, ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (const char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	//con_backscroll = 0; //johnfitz -- better console scrolling

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound ("misc/talk.wav");
	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}


		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}

	}
}


// borrowed from uhexen2 by S.A. for new procs, LOG_Init, LOG_Close

static char	logfilename[MAX_OSPATH];	// current logfile name
static int	log_fd = -1;			// log file descriptor

/*
================
Con_DebugLog
================
*/
void Con_DebugLog(const char *msg)
{
	if (log_fd == -1)
		return;

	write(log_fd, msg, strlen(msg));
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	4096
void Con_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;

	va_start (argptr,fmt);
	q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

// also echo to debugging console
	Sys_Printf ("%s", msg);

// log all messages to file
	if (con_debuglog)
		Con_DebugLog(msg);

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		return;		// no graphics mode

// write it to the scrollable buffer
	Con_Print (msg);

// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading )
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
}

/*
================
Con_Warning -- johnfitz -- prints a warning to the console
================
*/
void Con_Warning (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("\x02Warning: ");
	Con_Printf ("%s", msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("%s", msg); //johnfitz -- was Con_Printf
}

/*
================
Con_DPrintf2 -- johnfitz -- only prints if "developer" >= 2

currently not used
================
*/
void Con_DPrintf2 (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (developer.value >= 2)
	{
		va_start (argptr,fmt);
		q_vsnprintf (msg, sizeof(msg), fmt, argptr);
		va_end (argptr);

		Con_Printf ("%s", msg);
	}
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[1024];
	int			temp;

	va_start (argptr,fmt);
	q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
================
Con_CenterPrintf -- johnfitz -- pad each line with spaces to make it appear centered
================
*/
void Con_CenterPrintf (int linewidth, const char *fmt, ...) __attribute__((__format__(__printf__,2,3)));
void Con_CenterPrintf (int linewidth, const char *fmt, ...)
{
	va_list	argptr;
	char	msg[MAXPRINTMSG]; //the original message
	char	line[MAXPRINTMSG]; //one line from the message
	char	spaces[21]; //buffer for spaces
	char	*src, *dst;
	int		len, s;

	va_start (argptr,fmt);
	q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	linewidth = q_min(linewidth, con_linewidth);
	for (src = msg; *src; )
	{
		dst = line;
		while (*src && *src != '\n')
			*dst++ = *src++;
		*dst = 0;
		if (*src == '\n')
			src++;

		len = strlen(line);
		if (len < linewidth)
		{
			s = (linewidth-len)/2;
			memset (spaces, ' ', s);
			spaces[s] = 0;
			Con_Printf ("%s%s\n", spaces, line);
		}
		else
			Con_Printf ("%s\n", line);
	}
}

/*
==================
Con_LogCenterPrint -- johnfitz -- echo centerprint message to the console
==================
*/
void Con_LogCenterPrint (const char *str)
{
	if (!strcmp(str, con_lastcenterstring))
		return; //ignore duplicates

	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint.value != 2)
		return; //don't log in deathmatch

	strcpy(con_lastcenterstring, str);

	if (con_logcenterprint.value)
	{
		Con_Printf ("%s", Con_Quakebar(40));
		Con_CenterPrintf (40, "%s\n", str);
		Con_Printf ("%s", Con_Quakebar(40));
		Con_ClearNotify ();
	}
}

/*
==============================================================================

	TAB COMPLETION

==============================================================================
*/

//johnfitz -- tab completion stuff
//unique defs
char key_tabpartial[MAXCMDLINE];
typedef struct tab_s
{
	const char	*name;
	const char	*type;
	struct tab_s	*next;
	struct tab_s	*prev;
} tab_t;
tab_t	*tablist;

//defs from elsewhere
extern qboolean	keydown[256];
typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	const char		*name;
	xcommand_t		function;
} cmd_function_t;
extern	cmd_function_t	*cmd_functions;
#define	MAX_ALIAS_NAME	32
typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;
extern	cmdalias_t	*cmd_alias;

/*
============
AddToTabList -- johnfitz

tablist is a doubly-linked loop, alphabetized by name
============
*/

// bash_partial is the string that can be expanded,
// aka Linux Bash shell. -- S.A.
static char	bash_partial[80];
static qboolean	bash_singlematch;
static qboolean	map_singlematch;

void AddToTabList (const char *name, const char *type)
{
	tab_t	*t,*insert;
	char	*i_bash;
	const char *i_name;

	if (!*bash_partial)
	{
		strncpy (bash_partial, name, 79);
		bash_partial[79] = '\0';
	}
	else
	{
		bash_singlematch = 0;
		// find max common between bash_partial and name
		i_bash = bash_partial;
		i_name = name;
		while (*i_bash && (*i_bash == *i_name))
		{
			i_bash++;
			i_name++;
		}
		*i_bash = 0;
	}

	t = (tab_t *) Hunk_Alloc(sizeof(tab_t));
	t->name = name;
	t->type = type;

	if (!tablist) //create list
	{
		tablist = t;
		t->next = t;
		t->prev = t;
	}
	else if (strcmp(name, tablist->name) < 0) //insert at front
	{
		t->next = tablist;
		t->prev = tablist->prev;
		t->next->prev = t;
		t->prev->next = t;
		tablist = t;
	}
	else //insert later
	{
		insert = tablist;
		do
		{
			if (strcmp(name, insert->name) < 0)
				break;
			insert = insert->next;
		} while (insert != tablist);

		t->next = insert;
		t->prev = insert->prev;
		t->next->prev = t;
		t->prev->next = t;
	}
}

// This is redefined from host_cmd.c
typedef struct extralevel_s
{
	char			name[32];
	struct extralevel_s	*next;
} extralevel_t;

extern extralevel_t	*extralevels;

/*
============
BuildMap -- stevenaaus
============
*/
const char *BuildMapList (const char *partial)
{
	static char matched[80];
	char *i_matched, *i_name;
	extralevel_t	*level;
	int   init, match, plen;

	memset(matched, 0, 80);
	plen = strlen(partial);
	match = 0;

	for (level = extralevels, init = 0; level; level = level->next)
	{
		if (!strncmp(level->name, partial, plen))
		{
			if (init == 0)
			{
				init = 1;
				strncpy (matched, level->name, 79);
				matched[79] = '\0';
			}
			else
			{ // find max common
				i_matched = matched;
				i_name = level->name;
				while (*i_matched && (*i_matched == *i_name))
				{
					i_matched++;
					i_name++;
				}
				*i_matched = 0;
			}
			match++;
		}
	}

	map_singlematch = (match == 1);

	if (match > 1)
	{
		for (level = extralevels; level; level = level->next)
		{
			if (!strncmp(level->name, partial, plen))
				Con_SafePrintf ("   %s\n", level->name);
		}
		Con_SafePrintf ("\n");
	}

	return matched;
}

/*
============
BuildTabList -- johnfitz
============
*/
void BuildTabList (const char *partial)
{
	cmdalias_t		*alias;
	cvar_t			*cvar;
	cmd_function_t		*cmd;
	int				len;

	tablist = NULL;
	len = strlen(partial);

	bash_partial[0] = 0;
	bash_singlematch = 1;

	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_strncmp (partial, cvar->name, len))
			AddToTabList (cvar->name, "cvar");

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!Q_strncmp (partial,cmd->name, len))
			AddToTabList (cmd->name, "command");

	for (alias=cmd_alias ; alias ; alias=alias->next)
		if (!Q_strncmp (partial, alias->name, len))
			AddToTabList (alias->name, "alias");
}

/*
============
Con_TabComplete -- johnfitz
============
*/
void Con_TabComplete (void)
{
	char		partial[MAXCMDLINE];
	const char	*match;
	static char	*c;
	tab_t		*t;
	int			mark, i;

// if editline is empty, return
	if (key_lines[edit_line][1] == 0)
		return;

// get partial string (space -> cursor)
	if (!key_tabpartial[0]) //first time through, find new insert point. (Otherwise, use previous.)
	{
		//work back from cursor until you find a space, quote, semicolon, or prompt
		c = key_lines[edit_line] + key_linepos - 1; //start one space left of cursor
		while (*c!=' ' && *c!='\"' && *c!=';' && c!=key_lines[edit_line])
			c--;
		c++; //start 1 char after the separator we just found
	}
	for (i = 0; c + i < key_lines[edit_line] + key_linepos; i++)
		partial[i] = c[i];
	partial[i] = 0;

// Map autocomplete function -- S.A
// Since we don't have argument completion, this hack will do for now...
	if (!strncmp (key_lines[edit_line] + 1, "map ",4) ||
	    !strncmp (key_lines[edit_line] + 1, "changelevel ", 12))
	{
		const char *matched_map = BuildMapList(partial);
		if (!*matched_map)
			return;
		Q_strcpy (partial, matched_map);
		Q_strcpy (c, partial);
		key_linepos = c - key_lines[edit_line] + Q_strlen(matched_map); //set new cursor position
		// if only one match, append a space
		if ((key_lines[edit_line][key_linepos] == 0) && map_singlematch)
		{
			key_lines[edit_line][key_linepos] = ' ';
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
		}
		c = key_lines[edit_line] + key_linepos;
		return;
	}

//if partial is empty, return
	if (partial[0] == 0)
		return;

//trim trailing space becuase it screws up string comparisons
	if (i > 0 && partial[i-1] == ' ')
		partial[i-1] = 0;

// find a match
	mark = Hunk_LowMark();
	if (!key_tabpartial[0]) //first time through
	{
		Q_strcpy (key_tabpartial, partial);
		BuildTabList (key_tabpartial);

		if (!tablist)
			return;

		// print list if length > 1
		if (tablist->next != tablist)
		{
			t = tablist;
			Con_SafePrintf("\n");
			do
			{
				Con_SafePrintf("   %s (%s)\n", t->name, t->type);
				t = t->next;
			} while (t != tablist);
			Con_SafePrintf("\n");
		}

	//	match = tablist->name;
	// First time, just show maximum matching chars -- S.A.
		match = bash_partial;
	}
	else
	{
		BuildTabList (key_tabpartial);

		if (!tablist)
			return;

		//find current match -- can't save a pointer because the list will be rebuilt each time
		t = tablist;
		do
		{
			if (!Q_strcmp(t->name, partial))
				break;
			t = t->next;
		} while (t != tablist);

		//use prev or next to find next match
		match = keydown[K_SHIFT] ? t->prev->name : t->next->name;
	}
	Hunk_FreeToLowMark(mark); //it's okay to free it here because match is a pointer to persistent data

// insert new match into edit line
	Q_strcpy (partial, match); //first copy match string
	Q_strcat (partial, key_lines[edit_line] + key_linepos); //then add chars after cursor
	Q_strcpy (c, partial); //now copy all of this into edit line
	key_linepos = c - key_lines[edit_line] + Q_strlen(match); //set new cursor position

// if cursor is at end of string, let's append a space to make life easier
	if (key_lines[edit_line][key_linepos] == 0 && bash_singlematch)
	{
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	// S.A.: the map argument completion (may be in combination with the bash-style
	// display behavior changes, causes weirdness when completing the arguments for
	// the changelevel command. the line below "fixes" it, although I'm not sure about
	// the reason, yet, neither do I know any possible side effects of it:
		c = key_lines[edit_line] + key_linepos;
	}
}

/*
==============================================================================

DRAWING

==============================================================================
*/

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int	x, v;
	char	*text;
	int	i;
	float	time;
	extern	char chat_buffer[];

	GL_SetCanvas (CANVAS_CONSOLE); //johnfitz
	v = vid.conheight; //johnfitz

	for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines)*con_linewidth;

		clearnotify = 0;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, v, text[x]);

		v += 8;

		scr_tileclear_updates = 0; //johnfitz
	}

	if (key_dest == key_message)
	{
		// modified by S.A to support longer lines

		char	c[MAXCMDLINE+1]; // extra space == +1
		const char	*say_prompt;
		int	say_length, len;

		clearnotify = 0;
		x = 0;

		if (team_message)
			say_prompt = "say_team:";
		else
			say_prompt = "say:";

		say_length = strlen(say_prompt);

		Draw_String (8, v, say_prompt); //johnfitz

		text = strcpy(c, chat_buffer);
		len  = strlen(chat_buffer);
		text[len] = ' ';
		text[len+1] = 0;
		if (len >= con_linewidth - say_length)
			text += 1 + len + say_length - con_linewidth;

		while(*text)
		{
			Draw_Character ( (x+say_length+2)<<3, v, *text); //johnfitz
			x++; text++;
		}
		Draw_Character ( (x+say_length+1)<<3, v, 10+((int)(realtime*con_cursorspeed)&1)); 
		v += 8;

		scr_tileclear_updates = 0; //johnfitz
	}
}

/*
================
Con_DrawInput -- johnfitz -- modified to allow insert editing

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	extern	qpic_t *pic_ovr, *pic_ins; //johnfitz -- new cursor handling
	extern	double key_blinktime;
	extern	int key_insert;
	int	i, len;
	char	c[MAXCMDLINE], *text;

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = strcpy(c, key_lines[edit_line]);

// pad with one space so we can draw one past the string (in case the cursor is there)
	len = strlen(key_lines[edit_line]);
	text[len] = ' ';
	text[len+1] = 0;

// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

// draw input string
	for (i=0; i <= strlen(text) - 1; i++) //only write enough letters to go from *text to cursor
		Draw_Character ((i+1)<<3, vid.conheight - 16, text[i]);

// johnfitz -- new cursor handling
	if (!((int)((realtime-key_blinktime)*con_cursorspeed) & 1))
		Draw_Pic ((key_linepos+1)<<3, vid.conheight - 16, key_insert ? pic_ins : pic_ovr);
}

/*
================
Con_DrawConsole -- johnfitz -- heavy revision

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, qboolean drawinput)
{
	int				i, x, y, j, sb, rows;
	char			ver[32];
	char			*text;

	if (lines <= 0)
		return;

	con_vislines = lines * vid.conheight / glheight;
	GL_SetCanvas (CANVAS_CONSOLE);

// draw the background
	Draw_ConsoleBackground ();

// draw the buffer text
	rows = (con_vislines+7)/8;
	y = vid.conheight - rows*8;
	rows -= 2; //for input and version lines
	sb = (con_backscroll) ? 2 : 0;

	for (i = con_current - rows + 1 ; i <= con_current - sb ; i++, y+=8)
	{
		j = i - con_backscroll;
		if (j<0)
			j = 0;
		text = con_text + (j % con_totallines)*con_linewidth;

		for (x=0 ; x<con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, y, text[x]);
	}

// draw scrollback arrows
	if (con_backscroll)
	{
		y+=8; // blank line
		for (x=0 ; x<con_linewidth ; x+=4)
			Draw_Character ((x+1)<<3, y, '^');
		y+=8;
	}

// draw the input prompt, user text, and cursor
	if (drawinput)
		Con_DrawInput ();

//draw version number in bottom right
	y+=8;
	sprintf (ver, "QuakeSpasm %1.2f.%d", (float)FITZQUAKE_VERSION, QUAKESPASM_VER_PATCH);
	for (x=0; x<strlen(ver); x++)
		Draw_Character ((con_linewidth-strlen(ver)+x+2)<<3, y, ver[x] /*+ 128*/);
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (const char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf ("\n\n%s", Con_Quakebar(40)); //johnfitz
	Con_Printf ("%s", text);
	Con_Printf ("Press a key.\n");
	Con_Printf ("%s", Con_Quakebar(40)); //johnfitz

	key_count = -2;		// wait for a key down and up
	IN_Deactivate(vid.type == MODE_WINDOWED);
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	IN_Activate();
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}


void LOG_Init (quakeparms_t *parms)
{
	time_t	inittime;
	char	session[24];

	if (!COM_CheckParm("-condebug"))
		return;

	inittime = time (NULL);
	strftime (session, sizeof(session), "%m/%d/%Y %H:%M:%S", localtime(&inittime));
	q_snprintf (logfilename, sizeof(logfilename), "%s/qconsole.log", parms->basedir);

//	unlink (logfilename);

	log_fd = open (logfilename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (log_fd == -1)
	{
		fprintf (stderr, "Error: Unable to create log file %s\n", logfilename);
		return;
	}

	con_debuglog = true;
	Con_DebugLog (va("LOG started on: %s \n", session));

}

void LOG_Close (void)
{
	if (log_fd == -1)
		return;
	close (log_fd);
	log_fd = -1;
}

