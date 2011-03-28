/*
	snd_win.c
	$Id: snd_win.c,v 1.37 2008-12-28 14:34:34 sezero Exp $

	Copyright (C) 1996-1997  Id Software, Inc.

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
		51 Franklin St, Fifth Floor,
		Boston, MA  02110-1301  USA
*/

#define DX_DLSYM	/* dynamic loading of dsound symbols */

#include "quakedef.h"
#include "winquake.h"
#include <mmsystem.h>
#include <dsound.h>

//#define SNDBUFSIZE		65536
// 64K is > 1 second at 16-bit, 22050 Hz
//#define	WAV_BUFFERS		64 
#define	WAV_BUFFERS		128
#define	WAV_MASK		(WAV_BUFFERS - 1)

/* DirectSound : */
#ifndef DSBSIZE_MIN
#define DSBSIZE_MIN		4
#endif
#ifndef DSBSIZE_MAX
#define DSBSIZE_MAX		0x0FFFFFFF
#endif

static LPDIRECTSOUND		pDS;
static LPDIRECTSOUNDBUFFER	pDSBuf, pDSPBuf;

#if defined(DX_DLSYM)	/* dynamic loading of dsound symbols */
static HINSTANCE	hInstDS;
static HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
#else	/* ! DX_DLSYM : we're linked to dsound */
#define	pDirectSoundCreate		DirectSoundCreate
#endif	/* DX_DLSYM */

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL} sndinitstat;

static qboolean	wavonly;
static qboolean	dsound_init;
static qboolean	wav_init;
static qboolean	snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean	primary_format_set;

static int	sample16;
static int	snd_sent, snd_completed;
static int	ds_sbuf_size, wv_buf_size;

static HANDLE	hData;
static HGLOBAL	hWaveHdr;

static HPSTR	lpData;
static LPWAVEHDR	lpWaveHdr;
static HWAVEOUT	hWaveOut;
//WAVEOUTCAPS	wavecaps;

static DWORD	gSndBufSize;
static MMTIME	mmstarttime;


/*
==================
FreeSound
==================
*/
static void FreeSound (void)
{
	int		i;

	if (pDSBuf)
	{
		IDirectSoundBuffer_Stop(pDSBuf);
		IDirectSound_Release(pDSBuf);
	}

// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		IDirectSound_Release(pDSPBuf);
	}

	if (pDS)
	{
		IDirectSound_SetCooperativeLevel(pDS, mainwindow, DSSCL_NORMAL);
		IDirectSound_Release(pDS);
	}

	if (hWaveOut)
	{
		waveOutReset (hWaveOut);

		if (lpWaveHdr)
		{
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (hWaveOut);

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}
		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}


/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
static sndinitstat SNDDMA_InitDirect (dma_t *dma)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	memset((void *) dma, 0, sizeof(dma_t));
	shm = dma;

	shm->channels = 2; /* = desired_channels; */
	shm->samplebits = (loadas8bit.value) ? 8 : 16;
	shm->speed = sndspeed.value;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

#if defined(DX_DLSYM)
	if (!hInstDS)
	{
		hInstDS = LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_SafePrintf ("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (HRESULT (WINAPI *)(GUID FAR *, LPDIRECTSOUND FAR *, IUnknown FAR *))
								GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Con_SafePrintf ("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}
#endif	/* DX_DLSYM */

	hresult = pDirectSoundCreate(NULL, &pDS, NULL);
	if (hresult != DS_OK)
	{
		if (hresult != DSERR_ALLOCATED)
		{
			Con_SafePrintf ("DirectSound create failed\n");
			return SIS_FAILURE;
		}

		Con_SafePrintf ("DirectSoundCreate failure, hardware already in use\n");
		return SIS_NOTAVAIL;
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != IDirectSound_GetCaps(pDS, &dscaps))
	{
		Con_SafePrintf ("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf ("No DirectSound driver installed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	// if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, mainwindow, DSSCL_EXCLUSIVE))
	/* Pa3PyX: Some MIDI synthesizers are software and require access to
	   waveOut; so if we set the coop level to exclusive, MIDI will fail
	   to init because the device is locked. We use priority level instead.
	   That way we don't lock out software synths and other apps, but can
	   still set the sound buffer format. */
	if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, mainwindow, DSSCL_PRIORITY))
	{
		Con_SafePrintf ("Set coop level failed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

// get access to the primary buffer, if possible, so we can set the
// sound hardware format
	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat"))
	{
		if (DS_OK == IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != IDirectSoundBuffer_SetFormat(pDSPBuf, &pformat))
			{
				if (snd_firsttime)
					Con_SafePrintf ("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_SafePrintf ("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	if (!primary_format_set || !COM_CheckParm ("-primarysound"))
	{
	// create the secondary buffer we'll actually work with
		memset (&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		if (ds_sbuf_size < DSBSIZE_MIN)
			ds_sbuf_size = 1 << (Q_log2(DSBSIZE_MIN) + 1);
		if (ds_sbuf_size > DSBSIZE_MAX)
			ds_sbuf_size = 1 << Q_log2(DSBSIZE_MAX);
		dsbuf.dwBufferBytes = ds_sbuf_size;
		dsbuf.lpwfxFormat = &format;

		memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != IDirectSound_CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			Con_SafePrintf ("DS:CreateSoundBuffer Failed");
			FreeSound ();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->speed = format.nSamplesPerSec;

		if (DS_OK != IDirectSound_GetCaps(pDSBuf, &dsbcaps))
		{
			Con_SafePrintf ("DS:GetCaps failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (snd_firsttime)
			Con_SafePrintf ("Using secondary sound buffer\n");
	}
	else
	{
		if (DS_OK != IDirectSound_SetCooperativeLevel(pDS, mainwindow, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf ("Set coop level failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (DS_OK != IDirectSound_GetCaps(pDSPBuf, &dsbcaps))
		{
			Con_Printf ("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf ("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	if (snd_firsttime)
		Con_SafePrintf ("%lu bytes in sound buffer\n", (unsigned long)dsbcaps.dwBufferBytes);

	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = IDirectSoundBuffer_Lock(pDSBuf, 0, gSndBufSize, (LPVOID *) (HPSTR) &lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound ();
			return SIS_FAILURE;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound ();
			return SIS_FAILURE;
		}
	}

	memset(lpData, 0, dwSize);
//	lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	IDirectSoundBuffer_Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	IDirectSoundBuffer_Stop(pDSBuf);
	IDirectSoundBuffer_GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits / 8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
static qboolean SNDDMA_InitWav (dma_t *dma)
{
	WAVEFORMATEX	format;
	int			i;
	HRESULT			hr;

	snd_sent = 0;
	snd_completed = 0;

	memset((void *) dma, 0, sizeof(dma_t));
	shm = dma;

	shm->channels = 2; /* = desired_channels; */
	shm->samplebits = (loadas8bit.value) ? 8 : 16;
	shm->speed = sndspeed.value;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, &format, 0, 0L, CALLBACK_NULL);
	if (hr != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			Con_SafePrintf ("waveOutOpen failed\n");
			return false;
		}

		Con_SafePrintf ("waveOutOpen failure, hardware already in use\n");
		return false;
	}

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.
	 */
	gSndBufSize = WAV_BUFFERS * wv_buf_size;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_SafePrintf ("Sound: Out of memory.\n");
		FreeSound ();
		return false;
	}
	lpData = (HPSTR) GlobalLock(hData);
	if (!lpData)
	{
		Con_SafePrintf ("Sound: Failed to lock.\n");
		FreeSound ();
		return false;
	}
	memset (lpData, 0, gSndBufSize);

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);
	if (hWaveHdr == NULL)
	{
		Con_SafePrintf ("Sound: Failed to Alloc header.\n");
		FreeSound ();
		return false;
	}
	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr);
	if (lpWaveHdr == NULL)
	{
		Con_SafePrintf ("Sound: Failed to lock header.\n");
		FreeSound ();
		return false;
	}
	memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (i = 0; i < WAV_BUFFERS; i++)
	{
		lpWaveHdr[i].dwBufferLength = wv_buf_size;
		lpWaveHdr[i].lpData = lpData + i * wv_buf_size;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			Con_SafePrintf ("Sound: failed to prepare wave headers\n");
			FreeSound ();
			return false;
		}
	}

	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits / 8) - 1;

	wav_init = true;

	Con_SafePrintf ("%d sound buffers, %d bytes/sound buffer\n", WAV_BUFFERS, wv_buf_size);

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
qboolean SNDDMA_Init (dma_t *dma)
{
	sndinitstat	stat;
	int sndbits = (loadas8bit.value) ? 8 : 16;

	if (COM_CheckParm ("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Calculate Wave and DS buffer sizes to set, to store
	   2 secs of data, round up to the next power of 2  */
	ds_sbuf_size = 1 << (Q_log2((sndbits  >> 3) * ((int)sndspeed.value << 1)) + 1);
	wv_buf_size  = 1 << (Q_log2(((int)sndspeed.value << 3) / WAV_BUFFERS) + 1);

	/* Init DirectSound */
	if (!wavonly)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect (dma);

			if (stat == SIS_SUCCESS)
			{
				snd_isdirect = true;

				if (snd_firsttime)
					Con_SafePrintf ("DirectSound initialized\n");
			}
			else
			{
				snd_isdirect = false;
				Con_SafePrintf ("DirectSound failed to init\n");
			}
		}
	}

// if DirectSound didn't succeed in initializing, try to initialize
// waveOut sound, unless DirectSound failed because the hardware is
// already allocated (in which case the user has already chosen not
// to have sound)
	if (!dsound_init && (stat != SIS_NOTAVAIL))
	{
		if (snd_firsttime || snd_iswave)
		{
			snd_iswave = SNDDMA_InitWav (dma);

			if (snd_iswave)
			{
				if (snd_firsttime)
					Con_SafePrintf ("Wave sound initialized\n");
			}
			else
			{
				Con_SafePrintf ("Wave sound failed to init\n");
			}
		}
	}

	if (!dsound_init && !wav_init)
	{
		if (snd_firsttime)
			Con_SafePrintf ("No sound device initialized\n");

		snd_firsttime = false;

		return false;
	}

	snd_firsttime = false;

	return true;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos (void)
{
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	if (dsound_init)
	{
		mmtime.wType = TIME_SAMPLES;
		IDirectSoundBuffer_GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if (wav_init)
	{
		s = snd_sent * wv_buf_size;
	}
	else
	{	// we should not reach here...
		return 0;
	}

	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

/*
==============
SNDDMA_LockBuffer

Makes sure dma buffer is valid
===============
*/
static DWORD	locksize;
void SNDDMA_LockBuffer (void)
{
	if (pDSBuf)
	{
		void	*pData;
		int		reps;
		HRESULT	hresult;
		DWORD	dwStatus;

		reps = 0;
		shm->buffer = NULL;

		if (IDirectSoundBuffer_GetStatus(pDSBuf, &dwStatus) != DS_OK)
			Con_Printf ("Couldn't get sound buffer status\n");

		if (dwStatus & DSBSTATUS_BUFFERLOST)
			IDirectSoundBuffer_Restore(pDSBuf);

		if (!(dwStatus & DSBSTATUS_PLAYING))
			IDirectSoundBuffer_Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

		while ((hresult = IDirectSoundBuffer_Lock(pDSBuf, 0, gSndBufSize, (void **) &pData, &locksize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf ("SNDDMA_LockBuffer: DS::Lock Sound Buffer Failed\n");
				S_Shutdown ();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf ("SNDDMA_LockBuffer: DS: couldn't restore buffer\n");
				S_Shutdown ();
				return;
			}
		}

		shm->buffer = (unsigned char *) pData;
	}
}

/*
==============
SNDDMA_Submit

Unlock the dma buffer /
Send sound to the device
===============
*/
void SNDDMA_Submit (void)
{
	LPWAVEHDR	h;
	int			wResult;

	if (pDSBuf)
		IDirectSoundBuffer_Unlock(pDSBuf, shm->buffer, locksize, NULL, 0);

	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if ( snd_completed == snd_sent )
		{
			Con_DPrintf ("Sound overrun\n");
			break;
		}

		if ( ! (lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE) )
		{
			break;
		}

		snd_completed++;	// this buffer has been played
	}

	//
	// submit two new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < 4)
	{
		h = lpWaveHdr + (snd_sent & WAV_MASK);

		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/*
==================
SNDDMA_BlockSound
==================
*/
void SNDDMA_BlockSound (void)
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		waveOutReset (hWaveOut);
	}
}


/*
==================
SNDDMA_UnblockSound
==================
*/
void SNDDMA_UnblockSound (void)
{
}


/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown (void)
{
	FreeSound ();
#if defined(DX_DLSYM)
	if (hInstDS)
	{
		FreeLibrary(hInstDS);
		hInstDS = NULL;
	}
#endif	/* DX_DLSYM */
}

