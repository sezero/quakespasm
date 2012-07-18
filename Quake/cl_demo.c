/*
Copyright (C) 1996-2001 Id Software, Inc.

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

static void CL_FinishTimeDemo (void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	fclose (cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
static void CL_WriteDemoMessage (void)
{
	int	len;
	int	i;
	float	f;

	len = LittleLong (net_message.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	for (i = 0; i < 3; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		fwrite (&f, 4, 1, cls.demofile);
	}
	fwrite (net_message.data, net_message.cursize, 1, cls.demofile);
	fflush (cls.demofile);
}

static int CL_GetDemoMessage (void)
{
	int	r, i;
	float	f;

	// decide if it is time to grab the next message
	if (cls.signon == SIGNONS)	// always grab until fully connected
	{
		// Wait for full frame update on stufftext messages:
		// If the server stuffs a "reconnect", failing to wait
		// for the client to re-initialize before accepting
		// further messages freezes demo playback.  -- Pa3PyX
		if (host_framecount == cls.stufftext_frame)
			return 0;

		if (cls.timedemo)
		{
			if (host_framecount == cls.td_lastframe)
				return 0;	// already read this frame's message
			cls.td_lastframe = host_framecount;
		// if this is the second frame, grab the real td_starttime
		// so the bogus time on the first frame doesn't count
			if (host_framecount == cls.td_startframe + 1)
				cls.td_starttime = realtime;
		}
		else if (/* cl.time > 0 && */ cl.time <= cl.mtime[0])
		{
			return 0;	// don't need another message yet
		}
	}

// get the next message
	fread (&net_message.cursize, 4, 1, cls.demofile);
	VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
	for (i = 0 ; i < 3 ; i++)
	{
		r = fread (&f, 4, 1, cls.demofile);
		cl.mviewangles[0][i] = LittleFloat (f);
	}

	net_message.cursize = LittleLong (net_message.cursize);
	if (net_message.cursize > MAX_MSGLEN)
		Sys_Error ("Demo message > MAX_MSGLEN");
	r = fread (net_message.data, net_message.cursize, 1, cls.demofile);
	if (r != 1)
	{
		CL_StopPlayback ();
		return 0;
	}

	return 1;
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
	int	r;

	if (cls.demoplayback)
		return CL_GetDemoMessage ();

	while (1)
	{
		r = NET_GetMessage (cls.netcon);

		if (r != 1 && r != 2)
			return r;

	// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();

	return r;
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

// write a disconnect message to the demo file
	SZ_Clear (&net_message);
	MSG_WriteByte (&net_message, svc_disconnect);
	CL_WriteDemoMessage ();

// finish up
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> <map> [cd track]
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int		track;

	if (cmd_source != src_command)
		return;

	if (cls.demorecording)
		CL_Stop_f();

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
		Con_Printf("Can not record - already connected to server\nClient demo recording must be started before connecting\n");
		return;
	}

// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
	{
		track = -1;
	}

	q_snprintf (name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));

// start the map up
	if (c > 2)
	{
		Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);
		if (cls.state != ca_connected)
			return;
	}

// open the demo file
	COM_DefaultExtension (name, ".dem", sizeof(name));

	Con_Printf ("recording to %s.\n", name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't create %s\n", name);
		return;
	}

	cls.forcetrack = track;
	fprintf (cls.demofile, "%i\n", cls.forcetrack);

	cls.demorecording = true;
}


/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	char	name[MAX_OSPATH];
	int	c;
	qboolean neg;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

// disconnect from server
	CL_Disconnect ();

// open the demo file
	q_strlcpy (name, Cmd_Argv(1), sizeof(name));
	COM_DefaultExtension (name, ".dem", sizeof(name));

	Con_Printf ("Playing demo from %s.\n", name);

	COM_FOpenFile (name, &cls.demofile, NULL);
	if (!cls.demofile)
	{
		Con_Printf ("ERROR: couldn't open %s\n", name);
		cls.demonum = -1;	// stop demo loop
		return;
	}

// ZOID, fscanf is evil
// O.S.: if a space character e.g. 0x20 (' ') follows '\n',
// fscanf skips that byte too and screws up further reads.
//	fscanf (cls.demofile, "%i\n", &cls.forcetrack);
	cls.forcetrack = 0;
	neg = false;
	// read a decimal integer possibly with a leading '-',
	// followed by a '\n':
	for (i = 0; i < 13; i++)
	{
		c = getc(cls.demofile);
		if (c == '\n')
			break;
		if (c == '-') {
			neg = true;
			continue;
		}
		// check for multiple '-' or legal digits? meh...
		cls.forcetrack = cls.forcetrack * 10 + (c - '0');
	}
	if (c != '\n')
	{
		fclose (cls.demofile);
		cls.demofile = NULL;
		cls.demonum = -1;	// stop demo loop
		Con_Printf ("ERROR: demo \"%s\" is invalid\n", name);
		return;
	}
	if (neg)
		cls.forcetrack = -cls.forcetrack;

	cls.demoplayback = true;
	cls.state = ca_connected;

// get rid of the menu and/or console
	key_dest = key_game;

// Get a new message on playback start.
// Moved from CL_TimeDemo_f to here, Pa3PyX.
	cls.td_lastframe = -1;
}

/*
====================
CL_FinishTimeDemo

====================
*/
static void CL_FinishTimeDemo (void)
{
	int	frames;
	float	time;

	cls.timedemo = false;

// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();
	if (!cls.demofile)
		return;

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted

	cls.timedemo = true;
	cls.td_startframe = host_framecount;
//	cls.td_lastframe = -1;	// get a new message this frame
				// Moved to CL_PlayDemo_f(), Pa3PyX.
}

