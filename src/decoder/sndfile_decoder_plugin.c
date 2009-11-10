/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "decoder_api.h"
#include "audio_check.h"

#include <sndfile.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "sndfile"

static sf_count_t
sndfile_vio_get_filelen(void *user_data)
{
	const struct input_stream *is = user_data;

	return is->size;
}

static sf_count_t
sndfile_vio_seek(sf_count_t offset, int whence, void *user_data)
{
	struct input_stream *is = user_data;
	bool success;

	success = input_stream_seek(is, offset, whence);
	if (!success)
		return -1;

	return is->offset;
}

static sf_count_t
sndfile_vio_read(void *ptr, sf_count_t count, void *user_data)
{
	struct input_stream *is = user_data;
	size_t nbytes;

	nbytes = input_stream_read(is, ptr, count);
	if (nbytes == 0 && is->error != 0)
		return -1;

	return nbytes;
}

static sf_count_t
sndfile_vio_write(G_GNUC_UNUSED const void *ptr,
		  G_GNUC_UNUSED sf_count_t count,
		  G_GNUC_UNUSED void *user_data)
{
	/* no writing! */
	return -1;
}

static sf_count_t
sndfile_vio_tell(void *user_data)
{
	const struct input_stream *is = user_data;

	return is->offset;
}

/**
 * This SF_VIRTUAL_IO implementation wraps MPD's #input_stream to a
 * libsndfile stream.
 */
static SF_VIRTUAL_IO vio = {
	.get_filelen = sndfile_vio_get_filelen,
	.seek = sndfile_vio_seek,
	.read = sndfile_vio_read,
	.write = sndfile_vio_write,
	.tell = sndfile_vio_tell,
};

/**
 * Converts a frame number to a timestamp (in seconds).
 */
static float
frame_to_time(sf_count_t frame, const struct audio_format *audio_format)
{
	return (float)frame / (float)audio_format->sample_rate;
}

/**
 * Converts a timestamp (in seconds) to a frame number.
 */
static sf_count_t
time_to_frame(float t, const struct audio_format *audio_format)
{
	return (sf_count_t)(t * audio_format->sample_rate);
}

static void
sndfile_stream_decode(struct decoder *decoder, struct input_stream *is)
{
	GError *error = NULL;
	SNDFILE *sf;
	SF_INFO info;
	struct audio_format audio_format;
	size_t frame_size;
	sf_count_t read_frames, num_frames, position = 0;
	int buffer[4096];
	enum decoder_command cmd;

	info.format = 0;

	sf = sf_open_virtual(&vio, SFM_READ, &info, is);
	if (sf == NULL) {
		g_warning("sf_open_virtual() failed");
		return;
	}

	/* for now, always read 32 bit samples.  Later, we could lower
	   MPD's CPU usage by reading 16 bit samples with
	   sf_readf_short() on low-quality source files. */
	if (!audio_format_init_checked(&audio_format, info.samplerate, 32,
				       info.channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	decoder_initialized(decoder, &audio_format, info.seekable,
			    frame_to_time(info.frames, &audio_format));

	frame_size = audio_format_frame_size(&audio_format);
	read_frames = sizeof(buffer) / frame_size;

	do {
		num_frames = sf_readf_int(sf, buffer, read_frames);
		if (num_frames <= 0)
			break;

		cmd = decoder_data(decoder, is,
				   buffer, num_frames * frame_size,
				   frame_to_time(position, &audio_format),
				   0, NULL);
		if (cmd == DECODE_COMMAND_SEEK) {
			sf_count_t c =
				time_to_frame(decoder_seek_where(decoder),
					      &audio_format);
			c = sf_seek(sf, c, SEEK_SET);
			if (c < 0)
				decoder_seek_error(decoder);
			else
				decoder_command_finished(decoder);
			cmd = DECODE_COMMAND_NONE;
		}
	} while (cmd == DECODE_COMMAND_NONE);

	sf_close(sf);
}

static struct tag *
sndfile_tag_dup(const char *path_fs)
{
	SNDFILE *sf;
	SF_INFO info;
	struct tag *tag;
	const char *p;

	info.format = 0;

	sf = sf_open(path_fs, SFM_READ, &info);
	if (sf == NULL)
		return NULL;

	if (!audio_valid_sample_rate(info.samplerate)) {
		sf_close(sf);
		g_warning("Invalid sample rate in %s\n", path_fs);
		return NULL;
	}

	tag = tag_new();
	tag->time = info.frames / info.samplerate;

	p = sf_get_string(sf, SF_STR_TITLE);
	if (p != NULL)
		tag_add_item(tag, TAG_TITLE, p);

	p = sf_get_string(sf, SF_STR_ARTIST);
	if (p != NULL)
		tag_add_item(tag, TAG_ARTIST, p);

	p = sf_get_string(sf, SF_STR_DATE);
	if (p != NULL)
		tag_add_item(tag, TAG_DATE, p);

	sf_close(sf);

	return tag;
}

static const char *const sndfile_suffixes[] = {
	"wav", "aiff", "aif", /* Microsoft / SGI / Apple */
	"au", "snd", /* Sun / DEC / NeXT */
	"paf", /* Paris Audio File */
	"iff", "svx", /* Commodore Amiga IFF / SVX */
	"sf", /* IRCAM */
	"voc", /* Creative */
	"w64", /* Soundforge */
	"pvf", /* Portable Voice Format */
	"xi", /* Fasttracker */
	"htk", /* HMM Tool Kit */
	"caf", /* Apple */
	"sd2", /* Sound Designer II */

	/* libsndfile also supports FLAC and Ogg Vorbis, but only by
	   linking with libFLAC and libvorbis - we can do better, we
	   have native plugins for these libraries */

	NULL
};

static const char *const sndfile_mime_types[] = {
	"audio/x-wav",
	"audio/x-aiff",

	/* what are the MIME types of the other supported formats? */

	NULL
};

const struct decoder_plugin sndfile_decoder_plugin = {
	.name = "sndfile",
	.stream_decode = sndfile_stream_decode,
	.tag_dup = sndfile_tag_dup,
	.suffixes = sndfile_suffixes,
	.mime_types = sndfile_mime_types,
};
