/*
 * Ogg/Vorbis streaming music support, loosely based on several open source
 * Quake engine based projects with many modifications.
 *
 * Copyright (C) 2010-2011 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "quakedef.h"

#if defined(USE_CODEC_VORBIS)
#include "snd_codec.h"
#include "snd_codeci.h"
#include "snd_vorbis.h"

#define OV_EXCLUDE_STATIC_CALLBACKS
#if defined(VORBIS_USE_TREMOR)
/* for Tremor / Vorbisfile api differences,
 * see doc/diff.html in the Tremor package. */
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/vorbisfile.h>
#endif

/* The OGG codec can return the samples in a number of different
 * formats, we use the standard signed short format. */
#define OGG_SAMPLEWIDTH 2
#define OGG_SIGNED_DATA 1

/* CALLBACK FUNCTIONS: */

static int ovc_fclose (void *f)
{
	return 0;		/* we fclose() elsewhere. */
}

static int ovc_fseek (void *f, ogg_int64_t off, int whence)
{
	if (f == NULL) return (-1);
	return FS_fseek((fshandle_t *)f, (long) off, whence);
}

static const ov_callbacks ovc_qfs =
{
	(size_t (*)(void *, size_t, size_t, void *))	FS_fread,
	(int (*)(void *, ogg_int64_t, int))		ovc_fseek,
	(int (*)(void *))				ovc_fclose,
	(long (*)(void *))				FS_ftell
};

static qboolean S_OGG_CodecInitialize (void)
{
	return true;
}

static void S_OGG_CodecShutdown (void)
{
}

static snd_stream_t *S_OGG_CodecOpenStream (const char *filename)
{
	snd_stream_t *stream;
	OggVorbis_File *ovFile;
	vorbis_info *ogg_info;
	int res;

	stream = S_CodecUtilOpen(filename, &ogg_codec);
	if (!stream)
		return NULL;

	ovFile = (OggVorbis_File *) Z_Malloc(sizeof(OggVorbis_File));
	res = ov_open_callbacks(&stream->fh, ovFile, NULL, 0, ovc_qfs);
	if (res != 0)
	{
		Con_Printf("%s is not a valid Ogg Vorbis file (error %i).\n",
				filename, res);
		goto _fail;
	}

	stream->priv = ovFile;

	if (!ov_seekable(ovFile))
	{
		Con_Printf("OGG_Open: stream %s not seekable.\n", filename);
		goto _fail;
	}

	ogg_info = ov_info(ovFile, 0);
	if (!ogg_info)
	{
		Con_Printf("Unable to get stream information for %s.\n", filename);
		goto _fail;
	}

	if (ogg_info->channels != 1 && ogg_info->channels != 2)
	{
		Con_Printf("Unsupported number of channels %d in %s\n",
					ogg_info->channels, filename);
		goto _fail;
	}

	stream->info.rate = ogg_info->rate;
	stream->info.channels = ogg_info->channels;
	stream->info.width = OGG_SAMPLEWIDTH;

	return stream;
_fail:
	if (res == 0)
		ov_clear(ovFile);
	Z_Free(ovFile);
	S_CodecUtilClose(&stream);
	return NULL;
}

static int S_OGG_CodecReadStream (snd_stream_t *stream, int bytes, void *buffer)
{
	int	section;	/* FIXME: handle section changes */
	int	cnt, res, rem;
	char *	ptr;

	cnt = 0; rem = bytes;
	ptr = (char *) buffer;
	while (1)
	{
	/* # ov_read() from libvorbisfile returns the decoded PCM audio
	 *   in requested endianness, signedness and word size.
	 * # ov_read() from Tremor (libvorbisidec) returns decoded audio
	 *   always in host-endian, signed 16 bit PCM format.
	 * # For both of the libraries, if the audio is multichannel,
	 *   the channels are interleaved in the output buffer.
	 */
		res = ov_read( (OggVorbis_File *)stream->priv, ptr, rem,
#if !defined(VORBIS_USE_TREMOR)
				host_bigendian,
				OGG_SAMPLEWIDTH,
				OGG_SIGNED_DATA,
#endif	/* ! VORBIS_USE_TREMOR */
				&section );
		if (res <= 0)
			break;
		rem -= res;
		cnt += res;
		if (rem <= 0)
			break;
		ptr += res;
	}
	return (res >= 0) ? cnt : res;
}

static void S_OGG_CodecCloseStream (snd_stream_t *stream)
{
	ov_clear((OggVorbis_File *)stream->priv);
	Z_Free(stream->priv);
	S_CodecUtilClose(&stream);
}

static int S_OGG_CodecRewindStream (snd_stream_t *stream)
{
/* for libvorbisfile, the ov_time_seek() position argument
 * is seconds as doubles, whereas for Tremor libvorbisidec
 * it is milliseconds as 64 bit integers.
 */
	return ov_time_seek ((OggVorbis_File *)stream->priv, 0);
}

snd_codec_t ogg_codec =
{
	CODECTYPE_OGG,
	true,	/* always available. */
	"ogg",
	S_OGG_CodecInitialize,
	S_OGG_CodecShutdown,
	S_OGG_CodecOpenStream,
	S_OGG_CodecReadStream,
	S_OGG_CodecRewindStream,
	S_OGG_CodecCloseStream,
	NULL
};

#endif	/* USE_CODEC_VORBIS */

