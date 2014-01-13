/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "SndfileDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "InputStream.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <sndfile.h>

static constexpr Domain sndfile_domain("sndfile");

static sf_count_t
sndfile_vio_get_filelen(void *user_data)
{
	const InputStream &is = *(const InputStream *)user_data;

	return is.GetSize();
}

static sf_count_t
sndfile_vio_seek(sf_count_t offset, int whence, void *user_data)
{
	InputStream &is = *(InputStream *)user_data;

	if (!is.LockSeek(offset, whence, IgnoreError()))
		return -1;

	return is.GetOffset();
}

static sf_count_t
sndfile_vio_read(void *ptr, sf_count_t count, void *user_data)
{
	InputStream &is = *(InputStream *)user_data;

	Error error;
	size_t nbytes = is.LockRead(ptr, count, error);
	if (nbytes == 0 && error.IsDefined()) {
		LogError(error);
		return -1;
	}

	return nbytes;
}

static sf_count_t
sndfile_vio_write(gcc_unused const void *ptr,
		  gcc_unused sf_count_t count,
		  gcc_unused void *user_data)
{
	/* no writing! */
	return -1;
}

static sf_count_t
sndfile_vio_tell(void *user_data)
{
	const InputStream &is = *(const InputStream *)user_data;

	return is.GetOffset();
}

/**
 * This SF_VIRTUAL_IO implementation wraps MPD's #input_stream to a
 * libsndfile stream.
 */
static SF_VIRTUAL_IO vio = {
	sndfile_vio_get_filelen,
	sndfile_vio_seek,
	sndfile_vio_read,
	sndfile_vio_write,
	sndfile_vio_tell,
};

/**
 * Converts a frame number to a timestamp (in seconds).
 */
static float
frame_to_time(sf_count_t frame, const AudioFormat *audio_format)
{
	return (float)frame / (float)audio_format->sample_rate;
}

/**
 * Converts a timestamp (in seconds) to a frame number.
 */
static sf_count_t
time_to_frame(float t, const AudioFormat *audio_format)
{
	return (sf_count_t)(t * audio_format->sample_rate);
}

static void
sndfile_stream_decode(Decoder &decoder, InputStream &is)
{
	SNDFILE *sf;
	SF_INFO info;
	size_t frame_size;
	sf_count_t read_frames, num_frames;
	int buffer[4096];

	info.format = 0;

	sf = sf_open_virtual(&vio, SFM_READ, &info, &is);
	if (sf == nullptr) {
		LogWarning(sndfile_domain, "sf_open_virtual() failed");
		return;
	}

	/* for now, always read 32 bit samples.  Later, we could lower
	   MPD's CPU usage by reading 16 bit samples with
	   sf_readf_short() on low-quality source files. */
	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, info.samplerate,
				       SampleFormat::S32,
				       info.channels, error)) {
		LogError(error);
		return;
	}

	decoder_initialized(decoder, audio_format, info.seekable,
			    frame_to_time(info.frames, &audio_format));

	frame_size = audio_format.GetFrameSize();
	read_frames = sizeof(buffer) / frame_size;

	DecoderCommand cmd;
	do {
		num_frames = sf_readf_int(sf, buffer, read_frames);
		if (num_frames <= 0)
			break;

		cmd = decoder_data(decoder, is,
				   buffer, num_frames * frame_size,
				   0);
		if (cmd == DecoderCommand::SEEK) {
			sf_count_t c =
				time_to_frame(decoder_seek_where(decoder),
					      &audio_format);
			c = sf_seek(sf, c, SEEK_SET);
			if (c < 0)
				decoder_seek_error(decoder);
			else
				decoder_command_finished(decoder);
			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);

	sf_close(sf);
}

static bool
sndfile_scan_file(const char *path_fs,
		  const struct tag_handler *handler, void *handler_ctx)
{
	SNDFILE *sf;
	SF_INFO info;
	const char *p;

	info.format = 0;

	sf = sf_open(path_fs, SFM_READ, &info);
	if (sf == nullptr)
		return false;

	if (!audio_valid_sample_rate(info.samplerate)) {
		sf_close(sf);
		FormatWarning(sndfile_domain,
			      "Invalid sample rate in %s", path_fs);
		return false;
	}

	tag_handler_invoke_duration(handler, handler_ctx,
				    info.frames / info.samplerate);

	p = sf_get_string(sf, SF_STR_TITLE);
	if (p != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_TITLE, p);

	p = sf_get_string(sf, SF_STR_ARTIST);
	if (p != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_ARTIST, p);

	p = sf_get_string(sf, SF_STR_DATE);
	if (p != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_DATE, p);

	sf_close(sf);

	return true;
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

	nullptr
};

static const char *const sndfile_mime_types[] = {
	"audio/x-wav",
	"audio/x-aiff",

	/* what are the MIME types of the other supported formats? */

	nullptr
};

const struct DecoderPlugin sndfile_decoder_plugin = {
	"sndfile",
	nullptr,
	nullptr,
	sndfile_stream_decode,
	nullptr,
	sndfile_scan_file,
	nullptr,
	nullptr,
	sndfile_suffixes,
	sndfile_mime_types,
};
