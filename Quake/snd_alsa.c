/*
	snd_alsa.c

	ALSA 1.0 sound driver for Linux Hexen II

	Copyright (C) 1999,2004  contributors of the QuakeForge project

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
#include <alsa/asoundlib.h>

#define NB_PERIODS 4

//static const char alsa_default[] = "hw:0,0";
//static const char alsa_default[] = "plughw:0";
static const char alsa_default[] = "default";
static const char *pcmname = alsa_default;
static snd_pcm_t *pcm = NULL;
static snd_pcm_uframes_t buffer_size;

static const int	tryrates[] = { 11025, 22050, 44100, 48000, 16000, 24000, 8000 };
static const int	MAX_TRYRATES = sizeof(tryrates)/sizeof(tryrates[0]);


#if defined(__GNUC__) &&  \
  !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define ALSA_CHECK_ERR(check, fmt, args...)		\
    do {						\
	if (check < 0) {				\
		Con_Printf ("ALSA: " fmt, ##args);	\
		goto error;				\
	}						\
    } while (0)
#else
#define ALSA_CHECK_ERR(check, ...)			\
    do {						\
	if (check < 0) {				\
		Con_Printf ("ALSA: " __VA_ARGS__);	\
		goto error;				\
	}						\
    } while (0)
#endif

qboolean SNDDMA_Init (dma_t *dma)
{
	int			i, err;
	unsigned int		rate;
	int			tmp_bits, tmp_chan;
	snd_pcm_hw_params_t	*hw = NULL;
	snd_pcm_sw_params_t	*sw = NULL;
	snd_pcm_uframes_t	frag_size;

	i = COM_CheckParm("-alsadev");
	if (i != 0 && i < com_argc - 1)
		pcmname = com_argv[i + 1];

	err = snd_pcm_open (&pcm, pcmname, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0)
	{
		Con_Printf ("ALSA: error opening device \"%s\": %s\n", pcmname, snd_strerror(err));
		return false;
	}
	Con_Printf ("ALSA: Using device: %s\n", pcmname);

	err = snd_pcm_hw_params_malloc (&hw);
	ALSA_CHECK_ERR(err, "unable to allocate hardware params. %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_any (pcm, hw);
	ALSA_CHECK_ERR(err, "unable to init hardware params. %s\n", snd_strerror(err));

	err = snd_pcm_hw_params_set_access (pcm, hw, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	ALSA_CHECK_ERR(err, "unable to set interleaved access. %s\n", snd_strerror(err));

	i = (loadas8bit.value) ? 8 : 16;
	tmp_bits = (i == 8) ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16;
	err = snd_pcm_hw_params_set_format (pcm, hw, (snd_pcm_format_t) tmp_bits);
	if (err < 0)
	{
		Con_Printf ("Problems setting %d bit format, trying alternatives..\n", i);
		tmp_bits = (i == 8) ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_U8;
		err = snd_pcm_hw_params_set_format (pcm, hw, (snd_pcm_format_t) tmp_bits);
		ALSA_CHECK_ERR(err, "Neither 8 nor 16 bit format supported. %s\n", snd_strerror(err));
	}
	tmp_bits = (tmp_bits == SND_PCM_FORMAT_U8) ? 8 : 16;

	i = tmp_chan = (COM_CheckParm("-sndmono") == 0) ? 2 : 1;
	err = snd_pcm_hw_params_set_channels (pcm, hw, tmp_chan);
	if (err < 0)
	{
		Con_Printf ("Problems setting channels to %s, retrying for %s\n",
				(i == 2) ? "stereo" : "mono",
				(i == 2) ? "mono" : "stereo");
		tmp_chan = (i == 2) ? 1 : 2;
		err = snd_pcm_hw_params_set_channels (pcm, hw, tmp_chan);
		ALSA_CHECK_ERR(err, "unable to set desired channels. %s\n", snd_strerror(err));
	}

	rate = (int)sndspeed.value;
	err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
	if (err < 0)
	{
		Con_Printf("Problems setting sample rate, trying alternatives..\n");
		for (i = 0; i < MAX_TRYRATES; i++)
		{
			rate = tryrates[i];
			err = snd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (err < 0)
			{
				Con_DPrintf ("Unable to set sample rate %d\n", tryrates[i]);
				rate = 0;
			}
			else
			{
				if (rate != tryrates[i])
				{
					Con_Printf ("Warning: Rate set (%u) didn't match requested rate (%d)!\n", rate, tryrates[i]);
				//	goto error;
				}
				break;
			}
		}
		if (rate == 0)
		{
			Con_Printf ("Unable to set any sample rates.\n");
			goto error;
		}
	}
	else
	{
		if (rate != (int)sndspeed.value)
		{
			Con_Printf ("Warning: Rate set (%u) didn't match requested rate (%d)!\n", rate, (int)sndspeed.value);
		//	goto error;
		}
	}

	/* pick a buffer size that is a power of 2 (by masking off low bits) */
	buffer_size = i = (int)(rate * 0.15f);
	while (buffer_size & (buffer_size-1))
		buffer_size &= (buffer_size-1);
	/* then check if it is the nearest power of 2 and bump it up if not */
	if (i - buffer_size >= buffer_size >> 1)
		buffer_size *= 2;

	err = snd_pcm_hw_params_set_buffer_size_near (pcm, hw, &buffer_size);
	ALSA_CHECK_ERR(err, "unable to set buffer size near %lu (%s)\n",
				(unsigned long)buffer_size, snd_strerror(err));

	err = snd_pcm_hw_params_get_buffer_size (hw, &buffer_size);
	ALSA_CHECK_ERR(err, "unable to get buffer size. %s\n", snd_strerror(err));
	if (buffer_size & (buffer_size-1))
	{
		Con_Printf ("ALSA: WARNING: non-power of 2 buffer size. sound may be\n");
		Con_Printf ("unsatisfactory. Recommend using either the plughw or hw\n");
		Con_Printf ("devices or adjusting dmix to have a power of 2 buf size\n");
	}

	/* pick a period size near the buffer_size we got from ALSA */
	frag_size = buffer_size / NB_PERIODS;
	err = snd_pcm_hw_params_set_period_size_near (pcm, hw, &frag_size, 0);
	ALSA_CHECK_ERR(err, "unable to set period size near %i. %s\n",
				(int)frag_size, snd_strerror(err));

	err = snd_pcm_hw_params (pcm, hw);
	ALSA_CHECK_ERR(err, "unable to install hardware params. %s\n", snd_strerror(err));

	err = snd_pcm_sw_params_malloc (&sw);
	ALSA_CHECK_ERR(err, "unable to allocate software params. %s\n", snd_strerror(err));

	err = snd_pcm_sw_params_current (pcm, sw);
	ALSA_CHECK_ERR(err, "unable to determine current software params. %s\n", snd_strerror(err));

	err = snd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	ALSA_CHECK_ERR(err, "unable to set playback threshold. %s\n", snd_strerror(err));

	err = snd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);
	ALSA_CHECK_ERR(err, "unable to set playback stop threshold. %s\n", snd_strerror(err));

	err = snd_pcm_sw_params (pcm, sw);
	ALSA_CHECK_ERR(err, "unable to install software params. %s\n", snd_strerror(err));

	memset ((void *) dma, 0, sizeof(dma_t));
	shm = dma;

	shm->channels = tmp_chan;

	/*
	// don't mix less than this in mono samples:
	err = snd_pcm_hw_params_get_period_size (hw, 
			(snd_pcm_uframes_t *) (char *) (&shm->submission_chunk), 0);
	ALSA_CHECK_ERR(err, "unable to get period size. %s\n", snd_strerror(err));
	*/
	shm->submission_chunk = 1;
	shm->samplepos = 0;
	shm->samplebits = tmp_bits;

	Con_Printf ("ALSA: %lu bytes buffer with mmap interleaved access\n", (unsigned long)buffer_size);

	shm->samples = buffer_size * shm->channels; // mono samples in buffer
	shm->speed = rate;

	SNDDMA_GetDMAPos ();	// sets shm->buffer

	snd_pcm_hw_params_free(hw);
	snd_pcm_sw_params_free(sw);

	return true;

error:
// full clean-up
	if (hw)
		snd_pcm_hw_params_free(hw);
	if (sw)
		snd_pcm_sw_params_free(sw);
	shm = NULL;
	snd_pcm_close (pcm);
	pcm = NULL;
	return false;
}

int SNDDMA_GetDMAPos (void)
{
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes;
	const snd_pcm_channel_area_t *areas;

	if (!shm)
		return 0;

	nframes = shm->samples/shm->channels;
	snd_pcm_avail_update (pcm);
	snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
	// The following commit was absent in QF, causing the
	// very first sound to be corrupted
	snd_pcm_mmap_commit (pcm, offset, nframes);
	offset *= shm->channels;
	nframes *= shm->channels;
	shm->samplepos = offset;
	shm->buffer = (unsigned char *) areas->addr;	// FIXME! there's an area per channel
	return shm->samplepos;
}

void SNDDMA_Shutdown (void)
{
	if (shm)
	{
	// full clean-up
		Con_Printf ("Shutting down ALSA sound\n");
		snd_pcm_drop (pcm);	// do I need this?
		snd_pcm_close (pcm);
		pcm = NULL;
		shm->buffer = NULL;
		shm = NULL;
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
==============
*/
void SNDDMA_Submit (void)
{
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes;
	const snd_pcm_channel_area_t *areas;
	int state;
	int count = paintedtime - soundtime;

	nframes = count / shm->channels;
	snd_pcm_avail_update (pcm);
	snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
	state = snd_pcm_state (pcm);

	switch (state)
	{
	case SND_PCM_STATE_PREPARED:
		snd_pcm_mmap_commit (pcm, offset, nframes);
		snd_pcm_start (pcm);
		break;
	case SND_PCM_STATE_RUNNING:
		snd_pcm_mmap_commit (pcm, offset, nframes);
		break;
	default:
		break;
	}
}

void SNDDMA_BlockSound (void)
{
	snd_pcm_pause (pcm, 1);
}

void SNDDMA_UnblockSound (void)
{
	snd_pcm_pause (pcm, 0);
}

