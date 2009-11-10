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
#include "_ogg_common.h"
#include "audio_check.h"
#include "uri.h"

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
#include <stdlib.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "vorbis"
#define OGG_CHUNK_SIZE 4096

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define OGG_DECODE_USE_BIGENDIAN	1
#else
#define OGG_DECODE_USE_BIGENDIAN	0
#endif

struct vorbis_decoder_data {
	struct decoder *decoder;

	struct input_stream *input_stream;
	bool seekable;
};

static size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *vdata)
{
	struct vorbis_decoder_data *data = (struct vorbis_decoder_data *)vdata;
	size_t ret;

	ret = decoder_read(data->decoder, data->input_stream, ptr, size * nmemb);

	errno = 0;

	return ret / size;
}

static int ogg_seek_cb(void *vdata, ogg_int64_t offset, int whence)
{
	struct vorbis_decoder_data *data = (struct vorbis_decoder_data *)vdata;

	return data->seekable &&
		decoder_get_command(data->decoder) != DECODE_COMMAND_STOP &&
		input_stream_seek(data->input_stream, offset, whence)
		? 0 : -1;
}

/* TODO: check Ogg libraries API and see if we can just not have this func */
static int ogg_close_cb(G_GNUC_UNUSED void *vdata)
{
	return 0;
}

static long ogg_tell_cb(void *vdata)
{
	const struct vorbis_decoder_data *data =
		(const struct vorbis_decoder_data *)vdata;

	return (long)data->input_stream->offset;
}

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

static const char *
vorbis_comment_value(const char *comment, const char *needle)
{
	size_t len = strlen(needle);

	if (g_ascii_strncasecmp(comment, needle, len) == 0 &&
	    comment[len] == '=')
		return comment + len + 1;

	return NULL;
}

static struct replay_gain_info *
vorbis_comments_to_replay_gain(char **comments)
{
	struct replay_gain_info *rgi;
	const char *temp;
	bool found = false;

	rgi = replay_gain_info_new();

	while (*comments) {
		if ((temp =
		     vorbis_comment_value(*comments, "replaygain_track_gain"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_gain"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].gain = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_track_peak"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].peak = atof(temp);
			found = true;
		} else if ((temp = vorbis_comment_value(*comments,
							"replaygain_album_peak"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].peak = atof(temp);
			found = true;
		}

		comments++;
	}

	if (!found) {
		replay_gain_info_free(rgi);
		rgi = NULL;
	}

	return rgi;
}

static const char *VORBIS_COMMENT_TRACK_KEY = "tracknumber";
static const char *VORBIS_COMMENT_DISC_KEY = "discnumber";

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
vorbis_copy_comment(struct tag *tag, const char *comment,
		    const char *name, enum tag_type tag_type)
{
	const char *value;

	value = vorbis_comment_value(comment, name);
	if (value != NULL) {
		tag_add_item(tag, tag_type, value);
		return true;
	}

	return false;
}

static void
vorbis_parse_comment(struct tag *tag, const char *comment)
{
	assert(tag != NULL);

	if (vorbis_copy_comment(tag, comment, VORBIS_COMMENT_TRACK_KEY,
				TAG_TRACK) ||
	    vorbis_copy_comment(tag, comment, VORBIS_COMMENT_DISC_KEY,
				TAG_DISC) ||
	    vorbis_copy_comment(tag, comment, "album artist",
				TAG_ALBUM_ARTIST))
		return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(tag, comment,
					tag_item_names[i], i))
			return;
}

static struct tag *
vorbis_comments_to_tag(char **comments)
{
	struct tag *tag = tag_new();

	while (*comments)
		vorbis_parse_comment(tag, *comments++);

	if (tag_is_empty(tag)) {
		tag_free(tag);
		tag = NULL;
	}

	return tag;
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

static bool
oggvorbis_seekable(struct decoder *decoder)
{
	char *uri;
	bool seekable;

	uri = decoder_get_uri(decoder);
	/* disable seeking on remote streams, because libvorbis seeks
	   around like crazy, and due to being very expensive, this
	   delays song playback by 10 or 20 seconds */
	seekable = !uri_has_scheme(uri);
	g_free(uri);

	return seekable;
}

/* public */
static void
vorbis_stream_decode(struct decoder *decoder,
		     struct input_stream *input_stream)
{
	GError *error = NULL;
	OggVorbis_File vf;
	ov_callbacks callbacks;
	struct vorbis_decoder_data data;
	struct audio_format audio_format;
	float total_time;
	int current_section;
	int prev_section = -1;
	long ret;
	char chunk[OGG_CHUNK_SIZE];
	long bitRate = 0;
	long test;
	struct replay_gain_info *replay_gain_info = NULL;
	const vorbis_info *vi;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	if (ogg_stream_type_detect(input_stream) != VORBIS)
		return;

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_seek(input_stream, 0, SEEK_SET);

	data.decoder = decoder;
	data.input_stream = input_stream;
	data.seekable = input_stream->seekable && oggvorbis_seekable(decoder);

	callbacks.read_func = ogg_read_cb;
	callbacks.seek_func = ogg_seek_cb;
	callbacks.close_func = ogg_close_cb;
	callbacks.tell_func = ogg_tell_cb;
	if ((ret = ov_open_callbacks(&data, &vf, NULL, 0, callbacks)) < 0) {
		if (decoder_get_command(decoder) != DECODE_COMMAND_NONE)
			return;

		g_warning("Error decoding Ogg Vorbis stream: %s",
			  vorbis_strerror(ret));
		return;
	}

	vi = ov_info(&vf, -1);
	if (vi == NULL) {
		g_warning("ov_info() has failed");
		return;
	}

	if (!audio_format_init_checked(&audio_format, vi->rate, 16,
				       vi->channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	total_time = ov_time_total(&vf, -1);
	if (total_time < 0)
		total_time = 0;

	decoder_initialized(decoder, &audio_format, data.seekable, total_time);

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
			struct replay_gain_info *new_rgi;

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
			new_rgi = vorbis_comments_to_replay_gain(comments);
			if (new_rgi != NULL) {
				if (replay_gain_info != NULL)
					replay_gain_info_free(replay_gain_info);
				replay_gain_info = new_rgi;
			}

			prev_section = current_section;
		}

		if ((test = ov_bitrate_instant(&vf)) > 0)
			bitRate = test / 1000;

		cmd = decoder_data(decoder, input_stream,
				   chunk, ret,
				   ov_pcm_tell(&vf) / audio_format.sample_rate,
				   bitRate, replay_gain_info);
	} while (cmd != DECODE_COMMAND_STOP);

	if (replay_gain_info)
		replay_gain_info_free(replay_gain_info);

	ov_clear(&vf);
}

static struct tag *
vorbis_tag_dup(const char *file)
{
	struct tag *ret;
	FILE *fp;
	OggVorbis_File vf;

	fp = fopen(file, "rb");
	if (!fp) {
		return NULL;
	}

	if (ov_open(fp, &vf, NULL, 0) < 0) {
		fclose(fp);
		return NULL;
	}

	ret = vorbis_comments_to_tag(ov_comment(&vf, -1)->user_comments);

	if (!ret)
		ret = tag_new();
	ret->time = (int)(ov_time_total(&vf, -1) + 0.5);

	ov_clear(&vf);

	return ret;
}

static const char *const vorbis_suffixes[] = {
	"ogg", "oga", NULL
};

static const char *const vorbis_mime_types[] = {
	"application/ogg",
	"audio/x-vorbis+ogg",
	"application/x-ogg",
	NULL
};

const struct decoder_plugin vorbis_decoder_plugin = {
	.name = "vorbis",
	.stream_decode = vorbis_stream_decode,
	.tag_dup = vorbis_tag_dup,
	.suffixes = vorbis_suffixes,
	.mime_types = vorbis_mime_types
};
