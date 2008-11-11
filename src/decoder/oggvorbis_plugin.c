/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* TODO 'ogg' should probably be replaced with 'oggvorbis' in all instances */

#include "_ogg_common.h"

#ifndef HAVE_TREMOR
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
#include <errno.h>
#include <stdlib.h>

#ifdef WORDS_BIGENDIAN
#define OGG_DECODE_USE_BIGENDIAN	1
#else
#define OGG_DECODE_USE_BIGENDIAN	0
#endif

typedef struct _OggCallbackData {
	struct input_stream *inStream;
	struct decoder *decoder;
} OggCallbackData;

static size_t ogg_read_cb(void *ptr, size_t size, size_t nmemb, void *vdata)
{
	size_t ret;
	OggCallbackData *data = (OggCallbackData *) vdata;

	ret = decoder_read(data->decoder, data->inStream, ptr, size * nmemb);

	errno = 0;
	/*if(ret<0) errno = ((struct input_stream *)inStream)->error; */

	return ret / size;
}

static int ogg_seek_cb(void *vdata, ogg_int64_t offset, int whence)
{
	const OggCallbackData *data = (const OggCallbackData *) vdata;
	if(decoder_get_command(data->decoder) == DECODE_COMMAND_STOP)
		return -1;
	return input_stream_seek(data->inStream, offset, whence) ? 0 : -1;
}

/* TODO: check Ogg libraries API and see if we can just not have this func */
static int ogg_close_cb(mpd_unused void *vdata)
{
	return 0;
}

static long ogg_tell_cb(void *vdata)
{
	const OggCallbackData *data = (const OggCallbackData *) vdata;

	return (long)(data->inStream->offset);
}

static const char *ogg_parseComment(const char *comment, const char *needle)
{
	int len = strlen(needle);

	if (strncasecmp(comment, needle, len) == 0 && *(comment + len) == '=') {
		return comment + len + 1;
	}

	return NULL;
}

static struct replay_gain_info *
ogg_getReplayGainInfo(char **comments)
{
	struct replay_gain_info *rgi;
	const char *temp;
	bool found = false;

	rgi = replay_gain_info_new();

	while (*comments) {
		if ((temp =
		     ogg_parseComment(*comments, "replaygain_track_gain"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].gain = atof(temp);
			found = true;
		} else if ((temp = ogg_parseComment(*comments,
						    "replaygain_album_gain"))) {
			rgi->tuples[REPLAY_GAIN_ALBUM].gain = atof(temp);
			found = true;
		} else if ((temp = ogg_parseComment(*comments,
						    "replaygain_track_peak"))) {
			rgi->tuples[REPLAY_GAIN_TRACK].peak = atof(temp);
			found = true;
		} else if ((temp = ogg_parseComment(*comments,
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

static unsigned int ogg_parseCommentAddToTag(char *comment,
					     unsigned int itemType,
					     struct tag ** tag)
{
	const char *needle;
	unsigned int len;
	switch (itemType) {
	case TAG_ITEM_TRACK:
		needle = VORBIS_COMMENT_TRACK_KEY;
		break;
	case TAG_ITEM_DISC:
		needle = VORBIS_COMMENT_DISC_KEY;
		break;
	default:
		needle = mpdTagItemKeys[itemType];
	}
	len = strlen(needle);

	if (strncasecmp(comment, needle, len) == 0 && *(comment + len) == '=') {
		if (!*tag)
			*tag = tag_new();

		tag_add_item(*tag, itemType, comment + len + 1);

		return 1;
	}

	return 0;
}

static struct tag *oggCommentsParse(char **comments)
{
	struct tag *tag = NULL;

	while (*comments) {
		int j;
		for (j = TAG_NUM_OF_ITEM_TYPES; --j >= 0;) {
			if (ogg_parseCommentAddToTag(*comments, j, &tag))
				break;
		}
		comments++;
	}

	return tag;
}

static void putOggCommentsIntoOutputBuffer(struct decoder *decoder,
					   struct input_stream *is,
					   char **comments)
{
	struct tag *tag;

	tag = oggCommentsParse(comments);
	if (!tag)
		return;

	decoder_tag(decoder, is, tag);
	tag_free(tag);
}

/* public */
static void
oggvorbis_decode(struct decoder *decoder, struct input_stream *inStream)
{
	OggVorbis_File vf;
	ov_callbacks callbacks;
	OggCallbackData data;
	struct audio_format audio_format;
	int current_section;
	int prev_section = -1;
	long ret;
#define OGG_CHUNK_SIZE 4096
	char chunk[OGG_CHUNK_SIZE];
	long bitRate = 0;
	long test;
	struct replay_gain_info *replayGainInfo = NULL;
	char **comments;
	const char *errorStr;
	bool initialized = false;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	if (ogg_stream_type_detect(inStream) != VORBIS)
		return;

	/* rewind the stream, because ogg_stream_type_detect() has
	   moved it */
	input_stream_seek(inStream, 0, SEEK_SET);

	data.inStream = inStream;
	data.decoder = decoder;

	callbacks.read_func = ogg_read_cb;
	callbacks.seek_func = ogg_seek_cb;
	callbacks.close_func = ogg_close_cb;
	callbacks.tell_func = ogg_tell_cb;
	if ((ret = ov_open_callbacks(&data, &vf, NULL, 0, callbacks)) < 0) {
		if (decoder_get_command(decoder) != DECODE_COMMAND_NONE)
			return;

		switch (ret) {
		case OV_EREAD:
			errorStr = "read error";
			break;
		case OV_ENOTVORBIS:
			errorStr = "not vorbis stream";
			break;
		case OV_EVERSION:
			errorStr = "vorbis version mismatch";
			break;
		case OV_EBADHEADER:
			errorStr = "invalid vorbis header";
			break;
		case OV_EFAULT:
			errorStr = "internal logic error";
			break;
		default:
			errorStr = "unknown error";
			break;
		}

		g_warning("Error decoding Ogg Vorbis stream: %s\n",
			  errorStr);
		return;
	}
	audio_format.bits = 16;

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
		if (current_section != prev_section) {
			/*printf("new song!\n"); */
			vorbis_info *vi = ov_info(&vf, -1);
			struct replay_gain_info *new_rgi;

			audio_format.channels = vi->channels;
			audio_format.sample_rate = vi->rate;
			if (!initialized) {
				float total_time = ov_time_total(&vf, -1);
				if (total_time < 0)
					total_time = 0;
				decoder_initialized(decoder, &audio_format,
						    inStream->seekable,
						    total_time);
				initialized = true;
			}
			comments = ov_comment(&vf, -1)->user_comments;
			putOggCommentsIntoOutputBuffer(decoder, inStream,
						       comments);
			new_rgi = ogg_getReplayGainInfo(comments);
			if (new_rgi != NULL) {
				if (replayGainInfo != NULL)
					replay_gain_info_free(replayGainInfo);
				replayGainInfo = new_rgi;
			}
		}

		prev_section = current_section;

		if (ret <= 0) {
			if (ret == OV_HOLE) /* bad packet */
				ret = 0;
			else /* break on EOF or other error */
				break;
		}

		if ((test = ov_bitrate_instant(&vf)) > 0)
			bitRate = test / 1000;

		cmd = decoder_data(decoder, inStream,
				   chunk, ret,
				   ov_pcm_tell(&vf) / audio_format.sample_rate,
				   bitRate, replayGainInfo);
	} while (cmd != DECODE_COMMAND_STOP);

	if (replayGainInfo)
		replay_gain_info_free(replayGainInfo);

	ov_clear(&vf);
}

static struct tag *oggvorbis_TagDup(const char *file)
{
	struct tag *ret;
	FILE *fp;
	OggVorbis_File vf;

	fp = fopen(file, "r");
	if (!fp) {
		return NULL;
	}

	if (ov_open(fp, &vf, NULL, 0) < 0) {
		fclose(fp);
		return NULL;
	}

	ret = oggCommentsParse(ov_comment(&vf, -1)->user_comments);

	if (!ret)
		ret = tag_new();
	ret->time = (int)(ov_time_total(&vf, -1) + 0.5);

	ov_clear(&vf);

	return ret;
}

static const char *const oggvorbis_Suffixes[] = { "ogg","oga", NULL };
static const char *const oggvorbis_MimeTypes[] = {
	"application/ogg",
	"audio/x-vorbis+ogg",
	"application/x-ogg",
	NULL
};

const struct decoder_plugin oggvorbisPlugin = {
	.name = "oggvorbis",
	.stream_decode = oggvorbis_decode,
	.tag_dup = oggvorbis_TagDup,
	.suffixes = oggvorbis_Suffixes,
	.mime_types = oggvorbis_MimeTypes
};
