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

#include "q_stdinc.h"
#include "arch_def.h"
#include "net_sys.h"	/* for net_defs.h */
#include <dirent.h>
#include "quakedef.h"
#include "net_defs.h"	/* for struct qsocket_s details */

extern cvar_t	pausable;

extern int com_nummissionpacks; //johnfitz

int	current_skill;

void Mod_Print (void);

/*
==================
Host_Quit_f
==================
*/

void Host_Quit_f (void)
{
	if (key_dest != key_console && cls.state != ca_dedicated)
	{
		M_Menu_Quit_f ();
		return;
	}
	CL_Disconnect ();
	Host_ShutdownServer(false);

	Sys_Quit ();
}

//==============================================================================
//johnfitz -- dynamic gamedir stuff
//==============================================================================

// Declarations shared with common.c:
// FIXME: **** CLEAN THIS MESS!!! ***
typedef struct
{
	char	name[MAX_QPATH];
	int	filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	int		handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	unsigned int path_id;	// identifier assigned to the game directory
					// Note that <install_dir>/game1 and
					// <userdir>/game1 have the same id.
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

extern qboolean com_modified;
extern searchpath_t *com_searchpaths;
pack_t *COM_LoadPackFile (const char *packfile);

// Kill all the search packs until the game path is found. Kill it, then return
// the next path to it.
void KillGameDir(searchpath_t *search)
{
	searchpath_t *search_killer;
	while (search)
	{
		if (*search->filename)
		{
			com_searchpaths = search->next;
			Z_Free(search);
			return; //once you hit the dir, youve already freed the paks
		}
		Sys_FileClose (search->pack->handle); //johnfitz
		search_killer = search->next;
		Z_Free(search->pack->files);
		Z_Free(search->pack);
		Z_Free(search);
		search = search_killer;
	}
}

// Return the number of games in memory
int NumGames(searchpath_t *search)
{
	int found = 0;
	while (search)
	{
		if (*search->filename)
			found++;
		search = search->next;
	}
	return found;
}

void ExtraMaps_NewGame (void);

/*
==================
Host_Game_f
==================
*/
void Host_Game_f (void)
{
	int i;
	unsigned int path_id;
	searchpath_t *search = com_searchpaths;
	pack_t *pak;
	char   pakfile[MAX_OSPATH]; //FIXME: it's confusing to use this string for two different things

	if (Cmd_Argc() > 1)
	{
		if (!registered.value) //disable command for shareware quake
		{
			Con_Printf("You must have the registered version to use modified games\n");
			return;
		}

		if (strstr(Cmd_Argv(1), ".."))
		{
			Con_Printf ("Relative pathnames are not allowed.\n");
			return;
		}

		strcpy (pakfile, va("%s/%s", host_parms->basedir, Cmd_Argv(1)));
		if (!Q_strcasecmp(pakfile, com_gamedir)) //no change
		{
			Con_Printf("\"game\" is already \"%s\"\n", COM_SkipPath(com_gamedir));
			return;
		}

		com_modified = true;

		//Kill the server
		CL_Disconnect ();
		Host_ShutdownServer(true);

		//Write config file
		Host_WriteConfiguration ();

		//Kill the extra game if it is loaded
		if (NumGames(com_searchpaths) > 1 + com_nummissionpacks)
			KillGameDir(com_searchpaths);

		strcpy (com_gamedir, pakfile);

		if (Q_strcasecmp(Cmd_Argv(1), GAMENAME)) //game is not id1
		{
			// assign a path_id to this game directory
			if (com_searchpaths)
				path_id = com_searchpaths->path_id << 1;
			else	path_id = 1U;
			search = (searchpath_t *) Z_Malloc(sizeof(searchpath_t));
			search->path_id = path_id;
			strcpy (search->filename, pakfile);
			search->next = com_searchpaths;
			com_searchpaths = search;

			//Load the paks if any are found:
			for (i = 0; ; i++)
			{
				sprintf (pakfile, "%s/pak%i.pak", com_gamedir, i);
				pak = COM_LoadPackFile (pakfile);
				if (!pak)
					break;
				search = (searchpath_t *) Z_Malloc(sizeof(searchpath_t));
				search->path_id = path_id;
				search->pack = pak;
				search->next = com_searchpaths;
				com_searchpaths = search;
			}
		}

		//clear out and reload appropriate data
		Cache_Flush ();
		if (!isDedicated)
		{
			TexMgr_NewGame ();
			Draw_NewGame ();
			R_NewGame ();
		}
		ExtraMaps_NewGame ();
		//Cbuf_InsertText ("exec quake.rc\n");

		Con_Printf("\"game\" changed to \"%s\"\n", COM_SkipPath(com_gamedir));
	}
	else //Diplay the current gamedir
		Con_Printf("\"game\" is \"%s\"\n", COM_SkipPath(com_gamedir));
}

//==============================================================================
//johnfitz -- extramaps management
//==============================================================================

typedef struct extralevel_s
{
	char			name[32];
	struct extralevel_s	*next;
} extralevel_t;

extralevel_t	*extralevels;

void ExtraMaps_Add (const char *name)
{
	extralevel_t	*level,*cursor,*prev;

	// ignore duplicate
	for (level = extralevels; level; level = level->next)
	{
		if (!Q_strcmp (name, level->name))
			return;
	}

	level = (extralevel_t *) Z_Malloc(sizeof(extralevel_t));
	strcpy (level->name, name);

	// insert each entry in alphabetical order
	if (extralevels == NULL ||
	    Q_strcasecmp(level->name, extralevels->name) < 0) //insert at front
	{
		level->next = extralevels;
		extralevels = level;
	}
	else //insert later
	{
		prev = extralevels;
		cursor = extralevels->next;
		while (cursor && (Q_strcasecmp(level->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		level->next = prev->next;
		prev->next = level;
	}
}

void ExtraMaps_Init (void)
{
	DIR		*dir_p;
	struct dirent	*dir_t;
	char		filestring[MAX_OSPATH];
	char		mapname[32];
	char		ignorepakdir[32];
	searchpath_t	*search;
	pack_t		*pak;
	int		i;

	// we don't want to list the maps in id1 pakfiles,
	// because these are not "add-on" levels
	sprintf (ignorepakdir, "/%s/", GAMENAME);

	for (search = com_searchpaths; search; search = search->next)
	{
		if (*search->filename) //directory
		{
			sprintf (filestring,"%s/maps/",search->filename);
			dir_p = opendir(filestring);
			if (dir_p == NULL)
				continue;
			while ((dir_t = readdir(dir_p)) != NULL)
			{
				if (!strstr(dir_t->d_name, ".bsp") && !strstr(dir_t->d_name, ".BSP"))
					continue;
				COM_StripExtension(dir_t->d_name, mapname, sizeof(mapname));
				ExtraMaps_Add (mapname);
			}
			closedir(dir_p);
		}
		else //pakfile
		{
			if (!strstr(search->pack->filename, ignorepakdir))
			{ //don't list standard id maps
				for (i = 0, pak = search->pack; i < pak->numfiles; i++)
				{
					if (strstr(pak->files[i].name, ".bsp"))
					{
						if (pak->files[i].filelen > 32*1024)
						{ // don't list files under 32k (ammo boxes etc)
							COM_StripExtension(pak->files[i].name + 5, mapname, sizeof(mapname));
							ExtraMaps_Add (mapname);
						}
					}
				}
			}
		}
	}
}

void ExtraMaps_Clear (void)
{
	extralevel_t *blah;

	while (extralevels)
	{
		blah = extralevels->next;
		Z_Free(extralevels);
		extralevels = blah;
	}
}

void ExtraMaps_NewGame (void)
{
	ExtraMaps_Clear ();
	ExtraMaps_Init ();
}

/*
==================
Host_Maps_f
==================
*/
void Host_Maps_f (void)
{
	int i;
	extralevel_t	*level;

	for (level = extralevels, i = 0; level; level = level->next, i++)
		Con_SafePrintf ("   %s\n", level->name);

	if (i)
		Con_SafePrintf ("%i map(s)\n", i);
	else
		Con_SafePrintf ("no maps found\n");
}

//==============================================================================
//johnfitz -- modlist management
//==============================================================================

typedef struct mod_s
{
	char	name[MAX_OSPATH];
	struct mod_s	*next;
} mod_t;

mod_t	*modlist;

void Modlist_Add (const char *name)
{
	mod_t	*mod,*cursor,*prev;

	//ingore duplicate
	for (mod = modlist; mod; mod = mod->next)
	{
		if (!Q_strcmp (name, mod->name))
			return;
	}

	mod = (mod_t *) Z_Malloc(sizeof(mod_t));
	strcpy (mod->name, name);

	//insert each entry in alphabetical order
	if (modlist == NULL ||
	    Q_strcasecmp(mod->name, modlist->name) < 0) //insert at front
	{
		mod->next = modlist;
		modlist = mod;
	}
	else //insert later
	{
		prev = modlist;
		cursor = modlist->next;
		while (cursor && (Q_strcasecmp(mod->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		mod->next = prev->next;
		prev->next = mod;
	}
}

void Modlist_Init (void)
{
	DIR		*dir_p, *mod_dir_p;
	struct dirent	*dir_t, *mod_dir_t;
	qboolean	progs_found, pak_found;
	char		dir_string[MAX_OSPATH], mod_dir_string[MAX_OSPATH];
	int		i;

	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		sprintf (dir_string, "%s/", com_argv[i+1]);
	else
		sprintf (dir_string, "%s/", host_parms->basedir);

	dir_p = opendir(dir_string);
	if (dir_p == NULL)
		return;

	while ((dir_t = readdir(dir_p)) != NULL)
	{
		if ((strcmp(dir_t->d_name, ".") == 0) || (strcmp(dir_t->d_name, "..") == 0))
			continue;
		sprintf(mod_dir_string, "%s%s/", dir_string, dir_t->d_name);
		mod_dir_p = opendir(mod_dir_string);
		if (mod_dir_p == NULL)
			continue;
		progs_found = false;
		pak_found = false;
		// find progs.dat and pak file(s)
		while ((mod_dir_t = readdir(mod_dir_p)) != NULL)
		{
			if ((strcmp(mod_dir_t->d_name, ".") == 0) || (strcmp(mod_dir_t->d_name, "..") == 0))
				continue;
			if (Q_strcasecmp(mod_dir_t->d_name, "progs.dat") == 0)
				progs_found = true;
			if (strstr(mod_dir_t->d_name, ".pak") || strstr(mod_dir_t->d_name, ".PAK"))
				pak_found = true;
			if (progs_found || pak_found)
				break;
		}
		closedir(mod_dir_p);
		if (!progs_found && !pak_found)
			continue;
		Modlist_Add(dir_t->d_name);
	}

	closedir(dir_p);
}

/*
==================
Host_Mods_f -- johnfitz

list all potential mod directories (contain either a pak file or a progs.dat)
==================
*/
void Host_Mods_f (void)
{
	int i;
	mod_t	*mod;

	for (mod = modlist, i=0; mod; mod = mod->next, i++)
		Con_SafePrintf ("   %s\n", mod->name);

	if (i)
		Con_SafePrintf ("%i mod(s)\n", i);
	else
		Con_SafePrintf ("no mods found\n");
}

//==============================================================================

/*
=============
Host_Mapname_f -- johnfitz
=============
*/
void Host_Mapname_f (void)
{
	if (sv.active)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", sv.name);
		return;
	}

	if (cls.state == ca_connected)
	{
		Con_Printf ("\"mapname\" is \"%s\"\n", cl.mapname);
		return;
	}

	Con_Printf ("no map loaded\n");
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f (void)
{
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hours = 0;
	int			j;
	void		(*print_fn) (const char *fmt, ...) __fp_attribute__((__format__(__printf__,1,2)));

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
		print_fn = Con_Printf;
	}
	else
		print_fn = SV_ClientPrintf;

	print_fn ("host:    %s\n", Cvar_VariableString ("hostname"));
	print_fn ("version: %4.2f\n", VERSION);
	if (tcpipAvailable)
		print_fn ("tcp/ip:  %s\n", my_tcpip_address);
	if (ipxAvailable)
		print_fn ("ipx:     %s\n", my_ipx_address);
	print_fn ("map:     %s\n", sv.name);
	print_fn ("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int)(net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print_fn ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
		print_fn ("   %s\n", client->netconnection->address);
	}
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set god mode to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_GODMODE;
		if (!((int)sv_player->v.flags & FL_GODMODE) )
			SV_ClientPrintf ("godmode OFF\n");
		else
			SV_ClientPrintf ("godmode ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_GODMODE;
			SV_ClientPrintf ("godmode ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_GODMODE;
			SV_ClientPrintf ("godmode OFF\n");
		}
		break;
	default:
		Con_Printf("god [value] : toggle god mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Notarget_f
==================
*/
void Host_Notarget_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set notarget to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int)sv_player->v.flags ^ FL_NOTARGET;
		if (!((int)sv_player->v.flags & FL_NOTARGET) )
			SV_ClientPrintf ("notarget OFF\n");
		else
			SV_ClientPrintf ("notarget ON\n");
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int)sv_player->v.flags | FL_NOTARGET;
			SV_ClientPrintf ("notarget ON\n");
		}
		else
		{
			sv_player->v.flags = (int)sv_player->v.flags & ~FL_NOTARGET;
			SV_ClientPrintf ("notarget OFF\n");
		}
		break;
	default:
		Con_Printf("notarget [value] : toggle notarget mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

qboolean noclip_anglehack;

/*
==================
Host_Noclip_f
==================
*/
void Host_Noclip_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_NOCLIP)
		{
			noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf ("noclip ON\n");
		}
		else
		{
			noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("noclip OFF\n");
		}
		break;
	default:
		Con_Printf("noclip [value] : toggle noclip mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	case 2:
		if (Q_atof(Cmd_Argv(1)))
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf ("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf ("flymode OFF\n");
		}
		break;
	default:
		Con_Printf("fly [value] : toggle fly mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}


/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f (void)
{
	int		i, j;
	float		total;
	client_t	*client;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	SV_ClientPrintf ("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total+=client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf ("%4i %s\n", (int)(total*1000), client->name);
	}
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/


/*
======================
Host_Map_f

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f (void)
{
	int		i;
	char	name[MAX_QPATH], *p;

	if (Cmd_Argc() < 2)	//no map name given
	{
		if (cls.state == ca_dedicated)
		{
			if (sv.active)
				Con_Printf ("Current map: %s\n", sv.name);
			else
				Con_Printf ("Server not active\n");
		}
		else if (cls.state == ca_connected)
		{
			Con_Printf ("Current map: %s ( %s )\n", cl.levelname, cl.mapname);
		}
		else
		{
			Con_Printf ("map <levelname>: start a new server\n");
		}
		return;
	}

	if (cmd_source != src_command)
		return;

	cls.demonum = -1;		// stop demo loop in case this fails

	CL_Disconnect ();
	Host_ShutdownServer(false);

	if (cls.state != ca_dedicated)
		IN_Activate();
	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque ();

	svs.serverflags = 0;			// haven't completed an episode yet
	strcpy (name, Cmd_Argv(1));
	// remove (any) trailing ".bsp" from mapname S.A.
	p = strstr(name, ".bsp");
	if (p && p[4] == '\0')
		*p = '\0';
	SV_SpawnServer (name);
	if (!sv.active)
		return;

	if (cls.state != ca_dedicated)
	{
		strcpy (cls.spawnparms, "");

		for (i = 2; i < Cmd_Argc(); i++)
		{
			strcat (cls.spawnparms, Cmd_Argv(i));
			strcat (cls.spawnparms, " ");
		}

		Cmd_ExecuteString ("connect local", src_command);
	}
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f (void)
{
	char	level[MAX_QPATH];
	int		i; //johnfitz

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf ("Only the server may changelevel\n");
		return;
	}

	//johnfitz -- check for client having map before anything else
	sprintf (level, "maps/%s.bsp", Cmd_Argv(1));
	if (COM_OpenFile (level, &i, NULL) == -1)
		Host_Error ("cannot find map %s", level);
	//johnfitz

	if (cls.state != ca_dedicated)
		IN_Activate();	// -- S.A.
	key_dest = key_game;	// remove console or menu
	SV_SaveSpawnparms ();
	strcpy (level, Cmd_Argv(1));
	SV_SpawnServer (level);
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f (void)
{
	char	mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;
	strcpy (mapname, sv.name);	// must copy out, because it gets cleared
								// in sv_spawnserver
	SV_SpawnServer (mapname);
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void Host_Reconnect_f (void)
{
	SCR_BeginLoadingPlaque ();
	cls.signon = 0;		// need new connection messages
}

/*
=====================
Host_Connect_f

User command to connect to server
=====================
*/
void Host_Connect_f (void)
{
	char	name[MAX_QPATH];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback ();
		CL_Disconnect ();
	}
	strcpy (name, Cmd_Argv(1));
	CL_EstablishConnection (name);
	Host_Reconnect_f ();
}


/*
===============================================================================

LOAD / SAVE GAME

===============================================================================
*/

#define	SAVEGAME_VERSION	5

/*
===============
Host_SavegameComment

Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
===============
*/
void Host_SavegameComment (char *text)
{
	int		i;
	char	kills[20];

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		text[i] = ' ';
	memcpy (text, cl.levelname, q_min(strlen(cl.levelname),22)); //johnfitz -- only copy 22 chars.
	sprintf (kills,"kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy (text+22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
	{
		if (text[i] == ' ')
			text[i] = '_';
	}
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}


/*
===============
Host_Savegame_f
===============
*/
void Host_Savegame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	int	i;
	char	comment[SAVEGAME_COMMENT_LENGTH+1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf ("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf ("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf ("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0) )
		{
			Con_Printf ("Can't savegame with a dead player\n");
			return;
		}
	}

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav", sizeof(name));

	Con_Printf ("Saving game to %s...\n", name);
	f = fopen (name, "w");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fprintf (f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment (comment);
	fprintf (f, "%s\n", comment);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fprintf (f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf (f, "%d\n", current_skill);
	fprintf (f, "%s\n", sv.name);
	fprintf (f, "%f\n",sv.time);

// write the light styles

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			fprintf (f, "%s\n", sv.lightstyles[i]);
		else
			fprintf (f,"m\n");
	}


	ED_WriteGlobals (f);
	for (i = 0; i < sv.num_edicts; i++)
	{
		ED_Write (f, EDICT_NUM(i));
		fflush (f);
	}
	fclose (f);
	Con_Printf ("done.\n");
}


/*
===============
Host_Loadgame_f
===============
*/
void Host_Loadgame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	char	mapname[MAX_QPATH];
	float	time, tfloat;
	char	str[32768];
	const char  *start;
	int	i, r;
	edict_t	*ent;
	int	entnum;
	int	version;
	float	spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension (name, ".sav", sizeof(name));

// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Con_Printf ("Loading game from %s...\n", name);
	f = fopen (name, "r");
	if (!f)
	{
		Con_Printf ("ERROR: couldn't open.\n");
		return;
	}

	fscanf (f, "%i\n", &version);
	if (version != SAVEGAME_VERSION)
	{
		fclose (f);
		Con_Printf ("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}
	fscanf (f, "%s\n", str);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fscanf (f, "%f\n", &spawn_parms[i]);
// this silliness is so we can load 1.06 save files, which have float skill values
	fscanf (f, "%f\n", &tfloat);
	current_skill = (int)(tfloat + 0.1);
	Cvar_SetValue ("skill", (float)current_skill);

	fscanf (f, "%s\n",mapname);
	fscanf (f, "%f\n",&time);

	CL_Disconnect_f ();

	SV_SpawnServer (mapname);

	if (!sv.active)
	{
		fclose (f);
		Con_Printf ("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		fscanf (f, "%s\n", str);
		sv.lightstyles[i] = (const char *)Hunk_Strdup (str, "lightstyles");
	}

// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i = 0; i < sizeof(str) - 1; i++)
		{
			r = fgetc (f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i == sizeof(str) - 1)
		{
			fclose (f);
			Sys_Error ("Loadgame buffer overflow");
		}
		str[i] = 0;
		start = str;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token,"{"))
		{
			fclose (f);
			Sys_Error ("First token isn't a brace");
		}

		if (entnum == -1)
		{	// parse the global vars
			ED_ParseGlobals (start);
		}
		else
		{	// parse an edict

			ent = EDICT_NUM(entnum);
			memset (&ent->v, 0, progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict (start, ent);

		// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict (ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	fclose (f);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection ("local");
		Host_Reconnect_f ();
	}
}

//============================================================================

/*
======================
Host_Name_f
======================
*/
void Host_Name_f (void)
{
	char	newName[32];

	if (Cmd_Argc () == 1)
	{
		Con_Printf ("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}
	if (Cmd_Argc () == 2)
		Q_strncpy(newName, Cmd_Argv(1), sizeof(newName)-1);
	else
		Q_strncpy(newName, Cmd_Args(), sizeof(newName)-1);
	newName[15] = 0;	// client_t structure actually says name[32].

	if (cmd_source == src_command)
	{
		if (Q_strcmp(cl_name.string, newName) == 0)
			return;
		Cvar_Set ("_cl_name", newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected") )
	{
		if (Q_strcmp(host_client->name, newName) != 0)
			Con_Printf ("%s renamed to %s\n", host_client->name, newName);
	}
	Q_strcpy (host_client->name, newName);
	host_client->edict->v.netname = PR_SetEngineString(host_client->name);

// send notification to all clients

	MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString (&sv.reliable_datagram, host_client->name);
}

void Host_Say(qboolean teamonly)
{
	int		j;
	client_t	*client;
	client_t	*save;
	const char	*p;
	char		text[MAXCMDLINE], *p2;
	qboolean	quoted;
	qboolean	fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer ();
			return;
		}
	}

	if (Cmd_Argc () < 2)
		return;

	save = host_client;

	p = Cmd_Args();
// remove quotes if present
	quoted = false;
	if (*p == '\"')
	{
		p++;
		quoted = true;
	}
// turn on color set 1
	if (!fromServer)
		q_snprintf (text, sizeof(text), "\001%s: %s", save->name, p);
	else
		q_snprintf (text, sizeof(text), "\001<%s> %s", hostname.string, p);

// check length & truncate if necessary
	j = strlen(text);
	if (j >= sizeof(text) - 1)
	{
		text[sizeof(text) - 2] = '\n';
		text[sizeof(text) - 1] = '\0';
	}
	else
	{
		p2 = text + j;
		while ((const char *)p2 > (const char *)text &&
			(p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)) )
		{
			if (p2[-1] == '\"' && quoted)
				quoted = false;
			p2[-1] = '\0';
			p2--;
		}
		p2[0] = '\n';
		p2[1] = '\0';
	}

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
	}
	host_client = save;

	if (cls.state == ca_dedicated)
		Sys_Printf("%s", &text[1]);
}


void Host_Say_f(void)
{
	Host_Say(false);
}


void Host_Say_Team_f(void)
{
	Host_Say(true);
}


void Host_Tell_f(void)
{
	int		j;
	client_t	*client;
	client_t	*save;
	const char	*p;
	char		text[MAXCMDLINE], *p2;
	qboolean	quoted;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (Cmd_Argc () < 3)
		return;

	p = Cmd_Args();
// remove quotes if present
	quoted = false;
	if (*p == '\"')
	{
		p++;
		quoted = true;
	}
	q_snprintf (text, sizeof(text), "%s: %s", host_client->name, p);

// check length & truncate if necessary
	j = strlen(text);
	if (j >= sizeof(text) - 1)
	{
		text[sizeof(text) - 2] = '\n';
		text[sizeof(text) - 1] = '\0';
	}
	else
	{
		p2 = text + j;
		while ((const char *)p2 > (const char *)text &&
			(p2[-1] == '\r' || p2[-1] == '\n' || (p2[-1] == '\"' && quoted)) )
		{
			if (p2[-1] == '\"' && quoted)
				quoted = false;
			p2[-1] = '\0';
			p2--;
		}
		p2[0] = '\n';
		p2[1] = '\0';
	}

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned)
			continue;
		if (Q_strcasecmp(client->name, Cmd_Argv(1)))
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
		break;
	}
	host_client = save;
}


/*
==================
Host_Color_f
==================
*/
void Host_Color_f(void)
{
	int		top, bottom;
	int		playercolor;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"color\" is \"%i %i\"\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	playercolor = top*16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValue ("_cl_color", playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f (void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf ("Can't suicide -- allready dead!\n");
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram (pr_global_struct->ClientKill);
}


/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f (void)
{

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}
	if (!pausable.value)
		SV_ClientPrintf ("Pause not allowed.\n");
	else
	{
		sv.paused ^= 1;

		if (sv.paused)
		{
			SV_BroadcastPrintf ("%s paused the game\n", PR_GetString(sv_player->v.netname));
		}
		else
		{
			SV_BroadcastPrintf ("%s unpaused the game\n",PR_GetString(sv_player->v.netname));
		}

	// send notification to all clients
		MSG_WriteByte (&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte (&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================


/*
==================
Host_PreSpawn_f
==================
*/
void Host_PreSpawn_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("prespawn not valid -- allready spawned\n");
		return;
	}

	SZ_Write (&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 2);
	host_client->sendsignon = true;
}

/*
==================
Host_Spawn_f
==================
*/
void Host_Spawn_f (void)
{
	int		i;
	client_t	*client;
	edict_t	*ent;

	if (cmd_source == src_command)
	{
		Con_Printf ("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf ("Spawn not valid -- allready spawned\n");
		return;
	}

// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited allready
		// if this is the last client to be connected, unpause
		sv.paused = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		memset (&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = PR_SetEngineString(host_client->name);

		// copy spawn parms out of the client_t

		for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function

		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram (pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - host_client->netconnection->connecttime) <= sv.time)
			Sys_Printf ("%s entered the game\n", host_client->name);

		PR_ExecuteProgram (pr_global_struct->PutClientInServer);
	}


// send all current names, colors, and frag counts
	SZ_Clear (&host_client->message);

// send time of update
	MSG_WriteByte (&host_client->message, svc_time);
	MSG_WriteFloat (&host_client->message, sv.time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		MSG_WriteByte (&host_client->message, svc_updatename);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteString (&host_client->message, client->name);
		MSG_WriteByte (&host_client->message, svc_updatefrags);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteShort (&host_client->message, client->old_frags);
		MSG_WriteByte (&host_client->message, svc_updatecolors);
		MSG_WriteByte (&host_client->message, i);
		MSG_WriteByte (&host_client->message, client->colors);
	}

// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		MSG_WriteByte (&host_client->message, svc_lightstyle);
		MSG_WriteByte (&host_client->message, (char)i);
		MSG_WriteString (&host_client->message, sv.lightstyles[i]);
	}

//
// send some stats
//
	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_SECRETS);
	MSG_WriteLong (&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte (&host_client->message, svc_updatestat);
	MSG_WriteByte (&host_client->message, STAT_MONSTERS);
	MSG_WriteLong (&host_client->message, pr_global_struct->killed_monsters);


//
// send a fixangle
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM( 1 + (host_client - svs.clients) );
	MSG_WriteByte (&host_client->message, svc_setangle);
	for (i = 0; i < 2; i++)
		MSG_WriteAngle (&host_client->message, ent->v.angles[i] );
	MSG_WriteAngle (&host_client->message, 0 );

	SV_WriteClientdataToMessage (sv_player, &host_client->message);

	MSG_WriteByte (&host_client->message, svc_signonnum);
	MSG_WriteByte (&host_client->message, 3);
	host_client->sendsignon = true;
}

/*
==================
Host_Begin_f
==================
*/
void Host_Begin_f (void)
{
	if (cmd_source == src_command)
	{
		Con_Printf ("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;
}

//===========================================================================


/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f (void)
{
	const char	*who;
	const char	*message = NULL;
	client_t	*save;
	int		i;
	qboolean	byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer ();
			return;
		}
	}
	else if (pr_global_struct->deathmatch)
		return;

	save = host_client;

	if (Cmd_Argc() > 2 && Q_strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = Q_atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (Q_strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;							// skip the #
				while (*message == ' ')				// skip white space
					message++;
				message += Q_strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf ("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf ("Kicked by %s\n", who);
		SV_DropClient (false);
	}

	host_client = save;
}

/*
===============================================================================

DEBUGGING TOOLS

===============================================================================
*/

/*
==================
Host_Give_f
==================
*/
void Host_Give_f (void)
{
	const char	*t;
	int	v;
	eval_t	*val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer ();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	t = Cmd_Argv(1);
	v = atoi (Cmd_Argv(2));

	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (hipnotic)
		{
		    if (t[0] == '6')
		    {
			if (t[1] == 'a')
			    sv_player->v.items = (int)sv_player->v.items | HIT_PROXIMITY_GUN;
			else
			    sv_player->v.items = (int)sv_player->v.items | IT_GRENADE_LAUNCHER;
		    }
		    else if (t[0] == '9')
			sv_player->v.items = (int)sv_player->v.items | HIT_LASER_CANNON;
		    else if (t[0] == '0')
			sv_player->v.items = (int)sv_player->v.items | HIT_MJOLNIR;
		    else if (t[0] >= '2')
			sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
		    if (t[0] >= '2')
			sv_player->v.items = (int)sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_shells1");
		    if (val)
			val->_float = v;
		}
		sv_player->v.ammo_shells = v;
		break;

	case 'n':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_nails1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_nails = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_nails = v;
		}
		break;

	case 'l':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_lava_nails");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_nails = v;
		    }
		}
		break;

	case 'r':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_rockets1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_rockets = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_rockets = v;
		}
		break;

	case 'm':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_multi_rockets");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_rockets = v;
		    }
		}
		break;

	case 'h':
		sv_player->v.health = v;
		break;

	case 'c':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_cells1");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon <= IT_LIGHTNING)
			    sv_player->v.ammo_cells = v;
		    }
		}
		else
		{
		    sv_player->v.ammo_cells = v;
		}
		break;

	case 'p':
		if (rogue)
		{
		    val = GetEdictFieldValue(sv_player, "ammo_plasma");
		    if (val)
		    {
			val->_float = v;
			if (sv_player->v.weapon > IT_LIGHTNING)
			    sv_player->v.ammo_cells = v;
		    }
		}
		break;

	//johnfitz -- give armour
	case 'a':
		if (v > 150)
		{
		    sv_player->v.armortype = 0.8;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR3;
		}
		else if (v > 100)
		{
		    sv_player->v.armortype = 0.6;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR2;
		}
		else if (v >= 0)
		{
		    sv_player->v.armortype = 0.3;
		    sv_player->v.armorvalue = v;
		    sv_player->v.items = sv_player->v.items -
					((int)(sv_player->v.items) & (int)(IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) +
					IT_ARMOR1;
		}
		break;
		//johnfitz
	}

	//johnfitz -- update currentammo to match new ammo (so statusbar updates correctly)
	switch ((int)(sv_player->v.weapon))
	{
	case IT_SHOTGUN:
	case IT_SUPER_SHOTGUN:
		sv_player->v.currentammo = sv_player->v.ammo_shells;
		break;
	case IT_NAILGUN:
	case IT_SUPER_NAILGUN:
	case RIT_LAVA_SUPER_NAILGUN:
		sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case IT_GRENADE_LAUNCHER:
	case IT_ROCKET_LAUNCHER:
	case RIT_MULTI_GRENADE:
	case RIT_MULTI_ROCKET:
		sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	case IT_LIGHTNING:
	case HIT_LASER_CANNON:
	case HIT_MJOLNIR:
		sv_player->v.currentammo = sv_player->v.ammo_cells;
		break;
	case RIT_LAVA_NAILGUN: //same as IT_AXE
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_nails;
		break;
	case RIT_PLASMA_GUN: //same as HIT_PROXIMITY_GUN
		if (rogue)
			sv_player->v.currentammo = sv_player->v.ammo_cells;
		if (hipnotic)
			sv_player->v.currentammo = sv_player->v.ammo_rockets;
		break;
	}
	//johnfitz
}

edict_t	*FindViewthing (void)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		if ( !strcmp (PR_GetString(e->v.classname), "viewthing") )
			return e;
	}
	Con_Printf ("No viewthing on map\n");
	return NULL;
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = Mod_ForName (Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf ("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v.frame = 0;
	cl.model_precache[(int)e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f (void)
{
	edict_t	*e;
	int		f;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes - 1;

	e->v.frame = f;
}


void PrintFrameName (model_t *m, int frame)
{
	aliashdr_t 			*hdr;
	maliasframedesc_t	*pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata (m);
	if (!hdr)
		return;
	pframedesc = &hdr->frames[frame];

	Con_Printf ("frame %i: %s\n", frame, pframedesc->name);
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;
	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName (m, e->v.frame);
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f (void)
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing ();
	if (!e)
		return;

	m = cl.model_precache[(int)e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName (m, e->v.frame);
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/


/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f (void)
{
	int		i, c;

	if (cls.state == ca_dedicated)
		return;

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf ("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}
	Con_Printf ("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		strncpy (cls.demos[i-1], Cmd_Argv(i), sizeof(cls.demos[0])-1);

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		if (!fitzmode)
		{  /* QuakeSpasm customization: */
			/* go straight to menu, no CL_NextDemo */
			Cbuf_InsertText("menu_main\n");
			return;
		}
		CL_NextDemo ();
	}
	else
	{
		cls.demonum = -1;
	}
}


/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (cls.demonum == -1)
		cls.demonum = 1;
	CL_Disconnect_f ();
	CL_NextDemo ();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f (void)
{
	if (cls.state == ca_dedicated)
		return;
	if (!cls.demoplayback)
		return;
	CL_StopPlayback ();
	CL_Disconnect ();
}

//=============================================================================

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands (void)
{
	Cmd_AddCommand ("maps", Host_Maps_f); //johnfitz
	Cmd_AddCommand ("game", Host_Game_f); //johnfitz
	Cmd_AddCommand ("mods", Host_Mods_f); //johnfitz
	Cmd_AddCommand ("games", Host_Mods_f); // as an alias to "mods" -- S.A. / QuakeSpasm
	Cmd_AddCommand ("mapname", Host_Mapname_f); //johnfitz

	Cmd_AddCommand ("status", Host_Status_f);
	Cmd_AddCommand ("quit", Host_Quit_f);
	Cmd_AddCommand ("god", Host_God_f);
	Cmd_AddCommand ("notarget", Host_Notarget_f);
	Cmd_AddCommand ("fly", Host_Fly_f);
	Cmd_AddCommand ("map", Host_Map_f);
	Cmd_AddCommand ("restart", Host_Restart_f);
	Cmd_AddCommand ("changelevel", Host_Changelevel_f);
	Cmd_AddCommand ("connect", Host_Connect_f);
	Cmd_AddCommand ("reconnect", Host_Reconnect_f);
	Cmd_AddCommand ("name", Host_Name_f);
	Cmd_AddCommand ("noclip", Host_Noclip_f);

	Cmd_AddCommand ("say", Host_Say_f);
	Cmd_AddCommand ("say_team", Host_Say_Team_f);
	Cmd_AddCommand ("tell", Host_Tell_f);
	Cmd_AddCommand ("color", Host_Color_f);
	Cmd_AddCommand ("kill", Host_Kill_f);
	Cmd_AddCommand ("pause", Host_Pause_f);
	Cmd_AddCommand ("spawn", Host_Spawn_f);
	Cmd_AddCommand ("begin", Host_Begin_f);
	Cmd_AddCommand ("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand ("kick", Host_Kick_f);
	Cmd_AddCommand ("ping", Host_Ping_f);
	Cmd_AddCommand ("load", Host_Loadgame_f);
	Cmd_AddCommand ("save", Host_Savegame_f);
	Cmd_AddCommand ("give", Host_Give_f);

	Cmd_AddCommand ("startdemos", Host_Startdemos_f);
	Cmd_AddCommand ("demos", Host_Demos_f);
	Cmd_AddCommand ("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand ("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand ("viewframe", Host_Viewframe_f);
	Cmd_AddCommand ("viewnext", Host_Viewnext_f);
	Cmd_AddCommand ("viewprev", Host_Viewprev_f);

	Cmd_AddCommand ("mcache", Mod_Print);
}

