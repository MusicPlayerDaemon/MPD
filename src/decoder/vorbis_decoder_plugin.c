/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "vorbis_comments.h"
#include "_ogg_common.h"
#include "audio_check.h"
#include "uri.h"
#include "tag_handler.h"

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

#include <glib.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "vorbis"
#define OGG_CHUNK_SIZE 4096

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define OGG_DECODE_USE_BIGENDIAN	1
#else
#define OGG_DECODE_USE_BIGENDIAN	0
#endif

struct vorbis_input_stream {
	struct decoder *decoder;

	struct input_stream *input_stream;
	bool seekable;
};

static size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	struct vorbis_input_stream *vis = data;
	size_t ret;

	ret = decoder_read(vis->decoder, vis->input_stream, ptr, size * nmemb);

	errno = 0;

	return ret / size;
}

static int ogg_seek_cb(void *data, ogg_int64_t offset, int whence)
{
	struct vorbis_input_stream *vis = data;

	return vis->seekable &&
		(!vis->decoder || decoder_get_command(vis->decoder) != DECODE_COMMAND_STOP) &&
		input_stream_lock_seek(vis->input_stream, offset, whence, NULL)
		? 0 : -1;
}

/* TODO: check Ogg libraries API and see if we can just not have this func */
static int ogg_close_cb(G_GNUC_UNUSED void *data)
{
	return 0;
}

static long ogg_tell_cb(void *data)
{
	const struct vorbis_input_stream *vis = data;

	return (long)vis->input_stream->offset;
}

static const ov_callbacks vorbis_is_callbacks = {
	.read_func = ogg_read_cb,
	.seek_func = ogg_seek_cb,
	.close_func = ogg_close_cb,
	.tell_func = ogg_tell_cb,
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
vorbis_is_open(struct vorbis_input_stream *vis, OggVorbis_File *vf,
	       struct decoder *decoder, struct input_stream *input_stream)
{
	vis->decoder = decoder;
	vis->input_stream = input_stream;
	vis->seekable = input_stream->seekable &&
		(input_stream->uri == NULL ||
		 !uri_has_scheme(input_stream->uri));

	int ret = ov_open_callbacks(vis, vf, NULL, 0, vorbis_is_callbacks);
	if (ret < 0) {
		if (decoder == NULL ||
		    decoder_get_command(decoder) == DECODE_COMMAND_NONE)
			g_warning("Failed to open Ogg Vorbis stream: %s",
				  vorbis_strerror(ret));
		return false;
	}

	return true;
}

static void
vorbis_send_comments(struct decoder *decoder, struct input_stream *is,
		     char **comments)
{
	struct tag *tag;

	tag = vorbis_comments_to_tag(comments);
	if (!tag)
		return;

	decoder_tag(decoder, is, tag);
	tag_free(tag);
}

/* public */
static void
vorbis_stream_decode(struct decoder *decoder,
		     struct input_stream *input_stream)
{
	GError *error = NULL;
	OggVorbis_File vf;
	struct vorbis_input_stream vis;
	struct audio_format audio_format;
	float total_time;
	int current_section;
	int prev_section = -1;
	long ret;
	char chunk[OGG_CHUNK_SIZE];
	long bitRate = 0;
	long test;
	const vorbis_info *vi;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	if (ogg_stream_type_detect(input_stream) != VORBIS)
		return;

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_lock_seek(input_stream, 0, SEEK_SET, NULL);

	if (!vorbis_is_open(&vis, &vf, decoder, input_stream))
		return;

	vi = ov_info(&vf, -1);
	if (vi == NULL) {
		g_warning("ov_info() has failed");
		return;
	}

	if (!audio_format_init_checked(&audio_format, vi->rate,
				       SAMPLE_FORMAT_S16,
				       vi->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	total_time = ov_time_total(&vf, -1);
	if (total_time < 0)
		total_time = 0;

	decoder_initialized(decoder, &audio_format, vis.seekable, total_time);

	do {
		if (cmd == DECODE_COMMAND_SEEK) {
			double seek_where = decoder_seek_where(decoder);
			if (0 == ov_time_seek_page(&vf, seek_where)) {
				decoder_command_finished(decoder);
			} else
				decoder_seek_error(decoder);
		}

		ret = ov_read(&vf, chunk, sizeof(chunk),
			      OGG_DECODE_USE_BIGENDIAN, 2, 1, &current_section);
		if (ret == OV_HOLE) /* bad packet */
			ret = 0;
		else if (ret <= 0)
			/* break on EOF or other error */
			break;

		if (current_section != prev_section) {
			char **comments;

			vi = ov_info(&vf, -1);
			if (vi == NULL) {
				g_warning("ov_info() has failed");
				break;
			}

			if (vi->rate != (long)audio_format.sample_rate ||
			    vi->channels != (int)audio_format.channels) {
				/* we don't support audio format
				   change yet */
				g_warning("audio format change, stopping here");
				break;
			}

			comments = ov_comment(&vf, -1)->user_comments;
			vorbis_send_comments(decoder, input_stream, comments);

			struct replay_gain_info rgi;
			if (vorbis_comments_to_replay_gain(&rgi, comments))
				decoder_replay_gain(decoder, &rgi);

			prev_section = current_section;
		}

		if ((test = ov_bitrate_instant(&vf)) > 0)
			bitRate = test / 1000;

		cmd = decoder_data(decoder, input_stream,
				   chunk, ret,
				   bitRate);
	} while (cmd != DECODE_COMMAND_STOP);

	ov_clear(&vf);
}

static bool
vorbis_scan_stream(struct input_stream *is,
		   const struct tag_handler *handler, void *handler_ctx)
{
	struct vorbis_input_stream vis;
	OggVorbis_File vf;

	if (!vorbis_is_open(&vis, &vf, NULL, is))
		return false;

	tag_handler_invoke_duration(handler, handler_ctx,
				    (int)(ov_time_total(&vf, -1) + 0.5));

	vorbis_comments_scan(ov_comment(&vf, -1)->user_comments,
			     handler, handler_ctx);

	ov_clear(&vf);
	return true;
}

static const char *const vorbis_suffixes[] = {
	"ogg", "oga", NULL
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
	NULL
};

const struct decoder_plugin vorbis_decoder_plugin = {
	.name = "vorbis",
	.stream_decode = vorbis_stream_decode,
	.scan_stream = vorbis_scan_stream,
	.suffixes = vorbis_suffixes,
	.mime_types = vorbis_mime_types
};
