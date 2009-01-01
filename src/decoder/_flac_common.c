/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
 *
 * Common data structures and functions used by FLAC and OggFLAC
 * (c) 2005 by Eric Wong <normalperson@yhbt.net>
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

#include "_flac_common.h"

#include <glib.h>

#include <FLAC/format.h>
#include <FLAC/metadata.h>

void init_FlacData(FlacData * data, struct decoder * decoder,
		   struct input_stream *inStream)
{
	data->time = 0;
	data->position = 0;
	data->bitRate = 0;
	data->decoder = decoder;
	data->inStream = inStream;
	data->replayGainInfo = NULL;
	data->tag = NULL;
}

static int flacFindVorbisCommentFloat(const FLAC__StreamMetadata * block,
				      const char *cmnt, float *fl)
{
	int offset =
	    FLAC__metadata_object_vorbiscomment_find_entry_from(block, 0, cmnt);

	if (offset >= 0) {
		size_t pos = strlen(cmnt) + 1;	/* 1 is for '=' */
		int len = block->data.vorbis_comment.comments[offset].length
		    - pos;
		if (len > 0) {
			unsigned char tmp;
			unsigned char *p = &(block->data.vorbis_comment.
					     comments[offset].entry[pos]);
			tmp = p[len];
			p[len] = '\0';
			*fl = (float)atof((char *)p);
			p[len] = tmp;

			return 1;
		}
	}

	return 0;
}

/* replaygain stuff by AliasMrJones */
static void flacParseReplayGain(const FLAC__StreamMetadata * block,
				FlacData * data)
{
	int found = 0;

	if (data->replayGainInfo)
		replay_gain_info_free(data->replayGainInfo);

	data->replayGainInfo = replay_gain_info_new();

	found |= flacFindVorbisCommentFloat(block, "replaygain_album_gain",
					    &data->replayGainInfo->tuples[REPLAY_GAIN_ALBUM].gain);
	found |= flacFindVorbisCommentFloat(block, "replaygain_album_peak",
					    &data->replayGainInfo->tuples[REPLAY_GAIN_ALBUM].peak);
	found |= flacFindVorbisCommentFloat(block, "replaygain_track_gain",
					    &data->replayGainInfo->tuples[REPLAY_GAIN_TRACK].gain);
	found |= flacFindVorbisCommentFloat(block, "replaygain_track_peak",
					    &data->replayGainInfo->tuples[REPLAY_GAIN_TRACK].peak);

	if (!found) {
		replay_gain_info_free(data->replayGainInfo);
		data->replayGainInfo = NULL;
	}
}

/* tracknumber is used in VCs, MPD uses "track" ..., all the other
 * tag names match */
static const char *VORBIS_COMMENT_TRACK_KEY = "tracknumber";
static const char *VORBIS_COMMENT_DISC_KEY = "discnumber";

static unsigned int commentMatchesAddToTag(const
					   FLAC__StreamMetadata_VorbisComment_Entry
					   * entry, unsigned int itemType,
					   struct tag ** tag)
{
	const char *str;
	size_t slen;
	int vlen;

	switch (itemType) {
	case TAG_ITEM_TRACK:
		str = VORBIS_COMMENT_TRACK_KEY;
		break;
	case TAG_ITEM_DISC:
		str = VORBIS_COMMENT_DISC_KEY;
		break;
	default:
		str = mpdTagItemKeys[itemType];
	}
	slen = strlen(str);
	vlen = entry->length - slen - 1;

	if ((vlen > 0) && (0 == strncasecmp(str, (char *)entry->entry, slen))
	    && (*(entry->entry + slen) == '=')) {
		if (!*tag)
			*tag = tag_new();

		tag_add_item_n(*tag, itemType,
			       (char *)(entry->entry + slen + 1), vlen);

		return 1;
	}

	return 0;
}

struct tag *copyVorbisCommentBlockToMpdTag(const FLAC__StreamMetadata * block,
					   struct tag * tag)
{
	unsigned int i, j;
	FLAC__StreamMetadata_VorbisComment_Entry *comments;

	comments = block->data.vorbis_comment.comments;

	for (i = block->data.vorbis_comment.num_comments; i != 0; --i) {
		for (j = TAG_NUM_OF_ITEM_TYPES; j--;) {
			if (commentMatchesAddToTag(comments, j, &tag))
				break;
		}
		comments++;
	}

	return tag;
}

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     FlacData * data)
{
	const FLAC__StreamMetadata_StreamInfo *si = &(block->data.stream_info);

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		data->audio_format.bits = (int8_t)si->bits_per_sample;
		data->audio_format.sample_rate = si->sample_rate;
		data->audio_format.channels = (int8_t)si->channels;
		data->total_time = ((float)si->total_samples) / (si->sample_rate);
		break;
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flacParseReplayGain(block, data);
	default:
		break;
	}
}

void flac_error_common_cb(const char *plugin,
			  const FLAC__StreamDecoderErrorStatus status,
			  G_GNUC_UNUSED FlacData * data)
{
	if (decoder_get_command(data->decoder) == DECODE_COMMAND_STOP)
		return;

	switch (status) {
	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
		g_warning("%s lost sync\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		g_warning("bad %s header\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		g_warning("%s crc mismatch\n", plugin);
		break;
	default:
		g_warning("unknown %s error\n", plugin);
	}
}

static void flac_convert_stereo16(int16_t *dest,
				  const FLAC__int32 * const buf[],
				  unsigned int position, unsigned int end)
{
	for (; position < end; ++position) {
		*dest++ = buf[0][position];
		*dest++ = buf[1][position];
	}
}

static void
flac_convert_16(int16_t *dest,
		unsigned int num_channels,
		const FLAC__int32 * const buf[],
		unsigned int position, unsigned int end)
{
	unsigned int c_chan;

	for (; position < end; ++position)
		for (c_chan = 0; c_chan < num_channels; c_chan++)
			*dest++ = buf[c_chan][position];
}

/**
 * Note: this function also handles 24 bit files!
 */
static void
flac_convert_32(int32_t *dest,
		unsigned int num_channels,
		const FLAC__int32 * const buf[],
		unsigned int position, unsigned int end)
{
	unsigned int c_chan;

	for (; position < end; ++position)
		for (c_chan = 0; c_chan < num_channels; c_chan++)
			*dest++ = buf[c_chan][position];
}

static void
flac_convert_8(int8_t *dest,
	       unsigned int num_channels,
	       const FLAC__int32 * const buf[],
	       unsigned int position, unsigned int end)
{
	unsigned int c_chan;

	for (; position < end; ++position)
		for (c_chan = 0; c_chan < num_channels; c_chan++)
			*dest++ = buf[c_chan][position];
}

static void flac_convert(unsigned char *dest,
			 unsigned int num_channels,
			 unsigned int bytes_per_sample,
			 const FLAC__int32 * const buf[],
			 unsigned int position, unsigned int end)
{
	switch (bytes_per_sample) {
	case 2:
		if (num_channels == 2)
			flac_convert_stereo16((int16_t*)dest, buf,
					      position, end);
		else
			flac_convert_16((int16_t*)dest, num_channels, buf,
					position, end);
		break;

	case 4:
		flac_convert_32((int32_t*)dest, num_channels, buf,
				position, end);
		break;

	case 1:
		flac_convert_8((int8_t*)dest, num_channels, buf,
			       position, end);
		break;
	}
}

FLAC__StreamDecoderWriteStatus
flac_common_write(FlacData *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[])
{
	unsigned int c_samp;
	const unsigned int num_channels = frame->header.channels;
	const unsigned int bytes_per_sample =
		audio_format_sample_size(&data->audio_format);
	const unsigned int bytes_per_channel =
		bytes_per_sample * frame->header.channels;
	const unsigned int max_samples = FLAC_CHUNK_SIZE / bytes_per_channel;
	unsigned int num_samples;
	enum decoder_command cmd;

	if (bytes_per_sample != 1 && bytes_per_sample != 2 &&
	    bytes_per_sample != 4)
		/* exotic unsupported bit rate */
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	for (c_samp = 0; c_samp < frame->header.blocksize;
	     c_samp += num_samples) {
		num_samples = frame->header.blocksize - c_samp;
		if (num_samples > max_samples)
			num_samples = max_samples;

		flac_convert(data->chunk,
			     num_channels, bytes_per_sample, buf,
			     c_samp, c_samp + num_samples);

		cmd = decoder_data(data->decoder, data->inStream,
				   data->chunk,
				   num_samples * bytes_per_channel,
				   data->time, data->bitRate,
				   data->replayGainInfo);
		switch (cmd) {
		case DECODE_COMMAND_NONE:
		case DECODE_COMMAND_START:
			break;

		case DECODE_COMMAND_STOP:
			return
				FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

		case DECODE_COMMAND_SEEK:
			return
				FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
