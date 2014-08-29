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
#include "VorbisDecoderPlugin.h"
#include "VorbisComments.hxx"
#include "VorbisDomain.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "OggCodec.hxx"
#include "util/Error.hxx"
#include "util/Macros.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "Log.hxx"

#ifndef HAVE_TREMOR
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
#else
#include <tremor/ivorbisfile.h>
/* Macros to make Tremor's API look like libogg. Tremor always
   returns host-byte-order 16-bit signed data, and uses integer
   milliseconds where libogg uses double seconds.
*/
#define ov_read(VF, BUFFER, LENGTH, BIGENDIANP, WORD, SGNED, BITSTREAM) \
        ov_read(VF, BUFFER, LENGTH, BITSTREAM)
#define ov_time_total(VF, I) ((double)ov_time_total(VF, I)/1000)
#define ov_time_tell(VF) ((double)ov_time_tell(VF)/1000)
#define ov_time_seek_page(VF, S) (ov_time_seek_page(VF, (S)*1000))
#endif /* HAVE_TREMOR */

#include <errno.h>

struct VorbisInputStream {
	Decoder *const decoder;

	InputStream &input_stream;
	bool seekable;

	VorbisInputStream(Decoder *_decoder, InputStream &_is)
		:decoder(_decoder), input_stream(_is),
		 seekable(input_stream.CheapSeeking()) {}
};

static size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	VorbisInputStream *vis = (VorbisInputStream *)data;
	size_t ret = decoder_read(vis->decoder, vis->input_stream,
				  ptr, size * nmemb);

	errno = 0;

	return ret / size;
}

static int ogg_seek_cb(void *data, ogg_int64_t _offset, int whence)
{
	VorbisInputStream *vis = (VorbisInputStream *)data;
	InputStream &is = vis->input_stream;

	if (!vis->seekable ||
	    (vis->decoder != nullptr &&
	     decoder_get_command(*vis->decoder) == DecoderCommand::STOP))
		return -1;

	offset_type offset = _offset;
	switch (whence) {
	case SEEK_SET:
		break;

	case SEEK_CUR:
		offset += is.GetOffset();
		break;

	case SEEK_END:
		if (!is.KnownSize())
			return -1;

		offset += is.GetSize();
		break;

	default:
		return -1;
	}

	return is.LockSeek(offset, IgnoreError())
		? 0 : -1;
}

/* TODO: check Ogg libraries API and see if we can just not have this func */
static int ogg_close_cb(gcc_unused void *data)
{
	return 0;
}

static long ogg_tell_cb(void *data)
{
	VorbisInputStream *vis = (VorbisInputStream *)data;

	return (long)vis->input_stream.GetOffset();
}

static const ov_callbacks vorbis_is_callbacks = {
	ogg_read_cb,
	ogg_seek_cb,
	ogg_close_cb,
	ogg_tell_cb,
};

static const char *
vorbis_strerror(int code)
{
	switch (code) {
	case OV_EREAD:
		return "read error";

	case OV_ENOTVORBIS:
		return "not vorbis stream";

	case OV_EVERSION:
		return "vorbis version mismatch";

	case OV_EBADHEADER:
		return "invalid vorbis header";

	case OV_EFAULT:
		return "internal logic error";

	default:
		return "unknown error";
	}
}

static bool
vorbis_is_open(VorbisInputStream *vis, OggVorbis_File *vf)
{
	int ret = ov_open_callbacks(vis, vf, nullptr, 0, vorbis_is_callbacks);
	if (ret < 0) {
		if (vis->decoder == nullptr ||
		    decoder_get_command(*vis->decoder) == DecoderCommand::NONE)
			FormatWarning(vorbis_domain,
				      "Failed to open Ogg Vorbis stream: %s",
				      vorbis_strerror(ret));
		return false;
	}

	return true;
}

static void
vorbis_send_comments(Decoder &decoder, InputStream &is,
		     char **comments)
{
	Tag *tag = vorbis_comments_to_tag(comments);
	if (!tag)
		return;

	decoder_tag(decoder, is, std::move(*tag));
	delete tag;
}

#ifndef HAVE_TREMOR
static void
vorbis_interleave(float *dest, const float *const*src,
		  unsigned nframes, unsigned channels)
{
	for (const float *const*src_end = src + channels;
	     src != src_end; ++src, ++dest) {
		float *gcc_restrict d = dest;
		for (const float *gcc_restrict s = *src, *s_end = s + nframes;
		     s != s_end; ++s, d += channels)
			*d = *s;
	}
}
#endif

/* public */

static bool
vorbis_init(gcc_unused const config_param &param)
{
#ifndef HAVE_TREMOR
	LogDebug(vorbis_domain, vorbis_version_string());
#endif
	return true;
}

gcc_pure
static SignedSongTime
vorbis_duration(OggVorbis_File &vf)
{
	auto total = ov_time_total(&vf, -1);
	return total >= 0
		? SignedSongTime::FromS(total)
		: SignedSongTime::Negative();
}

static void
vorbis_stream_decode(Decoder &decoder,
		     InputStream &input_stream)
{
	if (ogg_codec_detect(&decoder, input_stream) != OGG_CODEC_VORBIS)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	input_stream.LockRewind(IgnoreError());

	VorbisInputStream vis(&decoder, input_stream);
	OggVorbis_File vf;
	if (!vorbis_is_open(&vis, &vf))
		return;

	const vorbis_info *vi = ov_info(&vf, -1);
	if (vi == nullptr) {
		LogWarning(vorbis_domain, "ov_info() has failed");
		return;
	}

	Error error;
	AudioFormat audio_format;
	if (!audio_format_init_checked(audio_format, vi->rate,
#ifdef HAVE_TREMOR
				       SampleFormat::S16,
#else
				       SampleFormat::FLOAT,
#endif
				       vi->channels, error)) {
		LogError(error);
		return;
	}

	decoder_initialized(decoder, audio_format, vis.seekable,
			    vorbis_duration(vf));

#ifdef HAVE_TREMOR
	char buffer[4096];
#else
	float buffer[2048];
	const int frames_per_buffer =
		ARRAY_SIZE(buffer) / audio_format.channels;
	const unsigned frame_size = sizeof(buffer[0]) * audio_format.channels;
#endif

	int prev_section = -1;
	unsigned kbit_rate = 0;

	DecoderCommand cmd = decoder_get_command(decoder);
	do {
		if (cmd == DecoderCommand::SEEK) {
			auto seek_where = decoder_seek_where_frame(decoder);
			if (0 == ov_pcm_seek_page(&vf, seek_where)) {
				decoder_command_finished(decoder);
			} else
				decoder_seek_error(decoder);
		}

		int current_section;

#ifdef HAVE_TREMOR
		long nbytes = ov_read(&vf, buffer, sizeof(buffer),
				      IsBigEndian(), 2, 1,
				      &current_section);
#else
		float **per_channel;
		long nframes = ov_read_float(&vf, &per_channel,
					     frames_per_buffer,
					     &current_section);
		long nbytes = nframes;
		if (nframes > 0) {
			vorbis_interleave(buffer,
					  (const float*const*)per_channel,
					  nframes, audio_format.channels);
			nbytes *= frame_size;
		}
#endif

		if (nbytes == OV_HOLE) /* bad packet */
			nbytes = 0;
		else if (nbytes <= 0)
			/* break on EOF or other error */
			break;

		if (current_section != prev_section) {
			vi = ov_info(&vf, -1);
			if (vi == nullptr) {
				LogWarning(vorbis_domain,
					   "ov_info() has failed");
				break;
			}

			if (vi->rate != (long)audio_format.sample_rate ||
			    vi->channels != (int)audio_format.channels) {
				/* we don't support audio format
				   change yet */
				LogWarning(vorbis_domain,
					   "audio format change, stopping here");
				break;
			}

			char **comments = ov_comment(&vf, -1)->user_comments;
			vorbis_send_comments(decoder, input_stream, comments);

			ReplayGainInfo rgi;
			if (vorbis_comments_to_replay_gain(rgi, comments))
				decoder_replay_gain(decoder, &rgi);

			prev_section = current_section;
		}

		long test = ov_bitrate_instant(&vf);
		if (test > 0)
			kbit_rate = test / 1000;

		cmd = decoder_data(decoder, input_stream,
				   buffer, nbytes,
				   kbit_rate);
	} while (cmd != DecoderCommand::STOP);

	ov_clear(&vf);
}

static bool
vorbis_scan_stream(InputStream &is,
		   const struct tag_handler *handler, void *handler_ctx)
{
	VorbisInputStream vis(nullptr, is);
	OggVorbis_File vf;

	if (!vorbis_is_open(&vis, &vf))
		return false;

	const auto total = ov_time_total(&vf, -1);
	if (total >= 0)
		tag_handler_invoke_duration(handler, handler_ctx,
					    SongTime::FromS(total));

	vorbis_comments_scan(ov_comment(&vf, -1)->user_comments,
			     handler, handler_ctx);

	ov_clear(&vf);
	return true;
}

static const char *const vorbis_suffixes[] = {
	"ogg", "oga", nullptr
};

static const char *const vorbis_mime_types[] = {
	"application/ogg",
	"application/x-ogg",
	"audio/ogg",
	"audio/vorbis",
	"audio/vorbis+ogg",
	"audio/x-ogg",
	"audio/x-vorbis",
	"audio/x-vorbis+ogg",
	nullptr
};

const struct DecoderPlugin vorbis_decoder_plugin = {
	"vorbis",
	vorbis_init,
	nullptr,
	vorbis_stream_decode,
	nullptr,
	nullptr,
	vorbis_scan_stream,
	nullptr,
	vorbis_suffixes,
	vorbis_mime_types
};
