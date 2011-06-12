/*
	snd_oss.c

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
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#include "quakedef.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
/* FIXME:  <sys/soundcard.h> is by the book, but we might
 * have to take care of <soundcard.h>, <linux/soundcard.h>
 * and <machine/soundcard.h> someday.  */
#include <sys/soundcard.h>
#include <errno.h>

static int		FORMAT_S16;

static int audio_fd = -1;
static const char oss_default[] = "/dev/dsp";
static const char *ossdev = oss_default;
static unsigned long mmaplen;

static const int	tryrates[] = { 11025, 22050, 44100, 48000, 16000, 24000, 8000 };
static const int	MAX_TRYRATES = sizeof(tryrates)/sizeof(tryrates[0]);


qboolean SNDDMA_Init (dma_t *dma)
{
	int		i, caps, tmp;
	unsigned long		sz;
	struct audio_buf_info	info;

	if (host_bigendian)	FORMAT_S16 = AFMT_S16_BE;
	else			FORMAT_S16 = AFMT_S16_LE;

	tmp = COM_CheckParm("-ossdev");
	if (tmp != 0 && tmp < com_argc - 1)
		ossdev = com_argv[tmp + 1];
	Con_Printf ("OSS: Using device: %s\n", ossdev);

// open /dev/dsp, confirm capability to mmap, and get size of dma buffer
	audio_fd = open(ossdev, O_RDWR|O_NONBLOCK);
	if (audio_fd == -1)
	{	// Failed open, retry up to 3 times if it's busy
		tmp = 3;
		while ( (audio_fd == -1) && tmp-- &&
			((errno == EAGAIN) || (errno == EBUSY)) )
		{
			sleep (1);
			audio_fd = open(ossdev, O_RDWR|O_NONBLOCK);
		}
		if (audio_fd == -1)
		{
			Con_Printf("Could not open %s. %s\n", ossdev, strerror(errno));
			return false;
		}
	}

	memset ((void *) dma, 0, sizeof(dma_t));
	shm = dma;

	if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) == -1)
	{
		Con_Printf("Could not reset %s. %s\n", ossdev, strerror(errno));
		goto error;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps) == -1)
	{
		Con_Printf("Couldn't retrieve soundcard capabilities. %s\n", strerror(errno));
		goto error;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf("Audio driver doesn't support mmap or trigger\n");
		goto error;
	}

// set sample bits & speed
	i = (loadas8bit.value) ? 8 : 16;
	tmp = (i == 16) ? FORMAT_S16 : AFMT_U8;
	if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp) == -1)
	{
		Con_Printf("Problems setting %d bit format, trying alternatives..\n", i);
		// try what the device gives us
		if (ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &tmp) == -1)
		{
			Con_Printf("Unable to retrieve supported formats. %s\n", strerror(errno));
			goto error;
		}
		if (tmp & FORMAT_S16)
		{
			i = 16;
			tmp = FORMAT_S16;
		}
		else if (tmp & AFMT_U8)
		{
			i = 8;
			tmp = AFMT_U8;
		}
		else
		{
			Con_Printf("Neither 8 nor 16 bit format supported.\n");
			goto error;
		}
		if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp) == -1)
		{
			Con_Printf("Unable to set sound format. %s\n", strerror(errno));
			goto error;
		}
	}
	shm->samplebits = i;

	tmp = (int)sndspeed.value;
	if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1)
	{
		Con_Printf("Problems setting sample rate, trying alternatives..\n");
		shm->speed = 0;
		for (i = 0; i < MAX_TRYRATES; i++)
		{
			tmp = tryrates[i];
			if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1)
			{
				Con_DPrintf ("Unable to set sample rate %d\n", tryrates[i]);
			}
			else
			{
				if (tmp != tryrates[i])
				{
					Con_Printf ("Warning: Rate set (%d) didn't match requested rate (%d)!\n", tmp, tryrates[i]);
				//	goto error;
				}
				shm->speed = tmp;
				break;
			}
		}
		if (shm->speed == 0)
		{
			Con_Printf("Unable to set any sample rates.\n");
			goto error;
		}
	}
	else
	{
		if (tmp != (int)sndspeed.value)
		{
			Con_Printf ("Warning: Rate set (%d) didn't match requested rate (%d)!\n", tmp, (int)sndspeed.value);
		//	goto error;
		}
		shm->speed = tmp;
	}

	i = (COM_CheckParm("-sndmono") == 0) ? 2 : 1;
	tmp = (i == 2) ? 1 : 0;
	if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp) == -1)
	{
		Con_Printf ("Problems setting channels to %s, retrying for %s\n",
				(i == 2) ? "stereo" : "mono",
				(i == 2) ? "mono" : "stereo");
		tmp = (i == 2) ? 0 : 1;
		if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp) == -1)
		{
			Con_Printf("unable to set desired channels. %s\n", strerror(errno));
			goto error;
		}
	}
	shm->channels = tmp +1;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1)
	{
		Con_Printf("Couldn't retrieve buffer status. %s\n", strerror(errno));
		goto error;
	}

	shm->samples = info.fragstotal * info.fragsize / (shm->samplebits / 8);
	shm->submission_chunk = 1;

// memory map the dma buffer
	sz = sysconf (_SC_PAGESIZE);
	mmaplen = info.fragstotal * info.fragsize;
	mmaplen = (mmaplen + sz - 1) & ~(sz - 1);
	shm->buffer = (unsigned char *) mmap(NULL, mmaplen, PROT_READ|PROT_WRITE,
					     MAP_FILE|MAP_SHARED, audio_fd, 0);
	if (!shm->buffer || shm->buffer == MAP_FAILED)
	{
		Con_Printf("Could not mmap %s. %s\n", ossdev, strerror(errno));
		goto error;
	}
	Con_Printf ("OSS: mmaped %lu bytes buffer\n", mmaplen);

// toggle the trigger & start her up
	tmp = 0;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) == -1)
	{
		Con_Printf("Could not toggle %s. %s\n", ossdev, strerror(errno));
		munmap (shm->buffer, mmaplen);
		goto error;
	}
	tmp = PCM_ENABLE_OUTPUT;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) == -1)
	{
		Con_Printf("Could not toggle %s. %s\n", ossdev, strerror(errno));
		munmap (shm->buffer, mmaplen);
		goto error;
	}

	shm->samplepos = 0;

	return true;

error:
	close(audio_fd);
	audio_fd = -1;
	shm->buffer = NULL;
	shm = NULL;
	return false;
}

int SNDDMA_GetDMAPos (void)
{
	struct count_info	count;

	if (!shm)
		return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count) == -1)
	{
		Con_Printf("Uh, sound dead. %s\n", strerror(errno));
		munmap (shm->buffer, mmaplen);
		shm->buffer = NULL;
		shm = NULL;
		close(audio_fd);
		audio_fd = -1;
		return 0;
	}
//	shm->samplepos = (count.bytes / (shm->samplebits / 8)) & (shm->samples-1);
//	fprintf(stderr, "%d    \r", count.ptr);
	shm->samplepos = count.ptr / (shm->samplebits / 8);

	return shm->samplepos;
}

void SNDDMA_Shutdown (void)
{
	int	tmp = 0;
	if (shm)
	{
		Con_Printf ("Shutting down OSS sound\n");
		munmap (shm->buffer, mmaplen);
		shm->buffer = NULL;
		shm = NULL;
		ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
		ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
		close(audio_fd);
		audio_fd = -1;
	}
}

/*
==============
SNDDMA_LockBuffer

Makes sure dma buffer is valid
==============
*/
void SNDDMA_LockBuffer (void)
{
	/* nothing to do here */
}

/*
==============
SNDDMA_Submit

Unlock the dma buffer /
Send sound to the device
===============
*/
void SNDDMA_Submit(void)
{
}

void SNDDMA_BlockSound (void)
{
}

void SNDDMA_UnblockSound (void)
{
}

