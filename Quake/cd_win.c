/*
	cd_win.c
	Win32 cdaudio code

	Copyright (C) 1996-1997 Id Software, Inc.
	Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
	rights reserved.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		51 Franklin Street, Fifth Floor,
		Boston, MA  02110-1301  USA
*/

#include "quakedef.h"
#include "winquake.h"
#include <mmsystem.h>

/*
 * You just can't set the volume of CD playback via MCI :
 * http://blogs.msdn.com/larryosterman/archive/2005/10/06/477874.aspx
 * OTOH, using the aux APIs to control the CD audio volume is broken.
 */
#undef	USE_AUX_API


static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static byte	remap[100];
static byte	playTrack;
static byte	maxTrack;

static float	old_cdvolume;
static UINT		wDeviceID;
static DWORD		end_pos;
#if defined(USE_AUX_API)
static UINT		CD_ID;
static unsigned long	CD_OrigVolume;
static void CD_SetVolume(unsigned long Volume);
#endif	/* USE_AUX_API */


static void CDAudio_Eject(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("MCI_SET_DOOR_OPEN failed (%u)\n", (unsigned int)dwReturn);
}


static void CDAudio_CloseDoor(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("MCI_SET_DOOR_CLOSED failed (%u)\n", (unsigned int)dwReturn);
}


static int CDAudio_GetAudioDiskInfo(void)
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;

	cdValid = false;

	mciStatusParms.dwItem = MCI_STATUS_READY;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: drive ready test - get status failed\n");
		return -1;
	}
	if (!mciStatusParms.dwReturn)
	{
		Con_DPrintf("CDAudio: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: get tracks - status failed\n");
		return -1;
	}
	if (mciStatusParms.dwReturn < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = mciStatusParms.dwReturn;

	return 0;
}


int CDAudio_Play(byte track, qboolean looping)
{
	DWORD				dwReturn;
	MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;

	if (!enabled)
		return -1;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return -1;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return -1;
	}

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%u)\n", (unsigned int)dwReturn);
		return -1;
	}
	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return -1;
	}

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%u)\n", (unsigned int)dwReturn);
		return -1;
	}

	if (playing)
	{
		if (playTrack == track)
			return 0;
		CDAudio_Stop();
	}

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
	end_pos = mciPlayParms.dwTo;
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%u)\n", (unsigned int)dwReturn);
		return -1;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (bgmvolume.value == 0) /* don't bother advancing */
		CDAudio_Pause ();

	return 0;
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

	if (!enabled)
		return;
	
	if (!playing)
		return;

	dwReturn = mciSendCommand(wDeviceID, MCI_STOP, 0, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("MCI_STOP failed (%u)", (unsigned int)dwReturn);

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	DWORD				dwReturn;
	MCI_GENERIC_PARMS	mciGenericParms;

	if (!enabled)
		return;

	if (!playing)
		return;

	mciGenericParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PAUSE, 0, (DWORD_PTR)(LPVOID) &mciGenericParms);
	if (dwReturn)
		Con_DPrintf("MCI_PAUSE failed (%u)", (unsigned int)dwReturn);

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	DWORD			dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;
	MCI_PLAY_PARMS	mciPlayParms;

	if (!enabled)
		return;
	if (!cdValid)
		return;
	if (!wasPlaying)
		return;

#if 0
/*	dwReturn = mciSendCommand(wDeviceID, MCI_RESUME, MCI_WAIT, NULL); */
	mciPlayParms.dwFrom = MCI_MAKE_TMSF(playTrack, 0, 0, 0);
	mciPlayParms.dwTo = MCI_MAKE_TMSF(playTrack + 1, 0, 0, 0);
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)(LPVOID) &mciPlayParms);
#endif
	mciStatusParms.dwItem = MCI_STATUS_POSITION;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("MCI_STATUS failed (%u)\n", (unsigned int)dwReturn);
		return;
	}
	mciPlayParms.dwFrom = mciStatusParms.dwReturn;
	mciPlayParms.dwTo = end_pos;	/* set in CDAudio_Play() */
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_FROM | MCI_TO | MCI_NOTIFY, (DWORD_PTR)(LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio: MCI_PLAY failed (%u)\n", (unsigned int)dwReturn);
		return;
	}
	playing = true;
}


static void CD_f (void)
{
	const char	*command;
	int		ret, n;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands:");
		Con_Printf("on, off, reset, remap, \n");
		Con_Printf("play, stop, loop, pause, resume\n");
		Con_Printf("eject, close, info\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (Q_strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv (n+1));
		return;
	}

	if (Q_strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (Q_strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", bgmvolume.value);
		return;
	}
}


LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != (LONG)wDeviceID)
		return 1;

	switch (wParam)
	{
		case MCI_NOTIFY_SUCCESSFUL:
			if (playing)
			{
				playing = false;
				if (playLooping)
					CDAudio_Play(playTrack, true);
			}
			break;

		case MCI_NOTIFY_ABORTED:
		case MCI_NOTIFY_SUPERSEDED:
			break;

		case MCI_NOTIFY_FAILURE:
			Con_DPrintf("MCI_NOTIFY_FAILURE\n");
			CDAudio_Stop ();
			cdValid = false;
			break;

		default:
			Con_DPrintf("Unexpected MM_MCINOTIFY type (%Iu)\n", wParam);
			return 1;
	}

	return 0;
}


static void CDAudio_SetVolume (float value)
{
	old_cdvolume = value;

	if (value == 0.0f)
		CDAudio_Pause ();
	else
		CDAudio_Resume();

#if defined(USE_AUX_API)
	CD_SetVolume (value * 0xffff);
#endif	/* USE_AUX_API */
}

void CDAudio_Update(void)
{
	if (!enabled)
		return;

	if (old_cdvolume != bgmvolume.value)
		CDAudio_SetVolume (bgmvolume.value);
}


#if defined(USE_AUX_API)
static void CD_FindCDAux(void)
{
	UINT		NumDevs, counter;
	MMRESULT		Result;
	AUXCAPS			Caps;

	CD_ID = -1;
	if (!COM_CheckParm("-usecdvolume"))
		return;
	NumDevs = auxGetNumDevs();
	for (counter = 0; counter < NumDevs; counter++)
	{
		Result = auxGetDevCaps(counter,&Caps,sizeof(Caps));
		if (!Result) // valid
		{
			if (Caps.wTechnology == AUXCAPS_CDAUDIO)
			{
				CD_ID = counter;
				auxGetVolume(CD_ID, &CD_OrigVolume);
				return;
			}
		}
	}
}

static void CD_SetVolume(unsigned long Volume)
{
	if (CD_ID != -1)
		auxSetVolume(CD_ID, (Volume<<16) + Volume);
}
#endif	/* USE_AUX_API */

static const char *get_cddev_arg (const char *arg)
{
/* arg should be like "D", "D:" or "D:\", make
 * sure it is so. Also check if this is really
 * a CDROM drive. */
	static char drive[4];
	if (!arg || ! *arg)
		return NULL;
	if (arg[1] != '\0')
	{
		if (arg[1] != ':')
			return NULL;
		if (arg[2] != '\0')
		{
			if (arg[2] != '\\' &&
			    arg[2] != '/')
				return NULL;
			if (arg[3] != '\0')
				return NULL;
		}
	}
	if (*arg >= 'A' && *arg <= 'Z')
	{
		drive[0] = *arg;
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
	}
	else if (*arg >= 'a' && *arg <= 'z')
	{
	/* make it uppercase */
		drive[0] = *arg - ('a' - 'A');
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
	}
	else
	{
		return NULL;
	}
	if (GetDriveType(drive) != DRIVE_CDROM)
	{
		Con_Printf("%c is not a CDROM drive\n", drive[0]);
		return NULL;
	}
	drive[2] = '\0';
	return drive;
}

int CDAudio_Init(void)
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
	MCI_SET_PARMS	mciSetParms;
	const char	*userdev = NULL;
	int	n;

	if (safemode || COM_CheckParm("-nocdaudio"))
		return -1;

	if ((n = COM_CheckParm("-cddev")) != 0 && n < com_argc - 1)
	{
		userdev = get_cddev_arg(com_argv[n + 1]);
		if (!userdev)
		{
			Con_Printf("Invalid argument to -cddev\n");
			return -1;
		}
		mciOpenParms.lpstrElementName = userdev;
	}

	mciOpenParms.lpstrDeviceType = "cdaudio";
	dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR) (LPVOID) &mciOpenParms);
	if (!userdev)
		userdev = "default cdrom";
	if (dwReturn)
	{
		Con_Printf("CDAudio_Init: MCI_OPEN failed for %s (%u)\n",
					userdev, (unsigned int)dwReturn);
		return -1;
	}
	wDeviceID = mciOpenParms.wDeviceID;

	// Set the time format to track/minute/second/frame (TMSF).
	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)(LPVOID) &mciSetParms);
	if (dwReturn)
	{
		Con_Printf("MCI_SET_TIME_FORMAT failed (%u)\n", (unsigned int)dwReturn);
		mciSendCommand(wDeviceID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		return -1;
	}

	for (n = 0; n < 100; n++)
		remap[n] = n;
	initialized = true;
	enabled = true;
	old_cdvolume = bgmvolume.value;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

#if defined(USE_AUX_API)
	CD_FindCDAux();
#endif	/* USE_AUX_API */

	Con_Printf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	if (mciSendCommand(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)NULL))
		Con_DPrintf("CDAudio_Shutdown: MCI_CLOSE failed\n");
}

