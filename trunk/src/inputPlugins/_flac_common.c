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

#include "../inputPlugin.h"

#if defined(HAVE_FLAC) || defined(HAVE_OGGFLAC)

#include "_flac_common.h"

#include "../log.h"
#include "../tag.h"
#include "../inputStream.h"
#include "../outputBuffer.h"
#include "../decode.h"
#include "../replayGain.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <FLAC/format.h>
#include <FLAC/metadata.h>

void init_FlacData(FlacData * data, OutputBuffer * cb,
		   DecoderControl * dc, InputStream * inStream)
{
	data->chunk_length = 0;
	data->time = 0;
	data->position = 0;
	data->bitRate = 0;
	data->cb = cb;
	data->dc = dc;
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
			unsigned char *dup = &(block->data.vorbis_comment.
					       comments[offset].entry[pos]);
			tmp = dup[len];
			dup[len] = '\0';
			*fl = atof((char *)dup);
			dup[len] = tmp;

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
		freeReplayGainInfo(data->replayGainInfo);

	data->replayGainInfo = newReplayGainInfo();

	found |= flacFindVorbisCommentFloat(block, "replaygain_album_gain",
					    &data->replayGainInfo->albumGain);
	found |= flacFindVorbisCommentFloat(block, "replaygain_album_peak",
					    &data->replayGainInfo->albumPeak);
	found |= flacFindVorbisCommentFloat(block, "replaygain_track_gain",
					    &data->replayGainInfo->trackGain);
	found |= flacFindVorbisCommentFloat(block, "replaygain_track_peak",
					    &data->replayGainInfo->trackPeak);

	if (!found) {
		freeReplayGainInfo(data->replayGainInfo);
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
					   MpdTag ** tag)
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
			*tag = newMpdTag();

		addItemToMpdTagWithLen(*tag, itemType,
				       (char *)(entry->entry + slen + 1), vlen);

		return 1;
	}

	return 0;
}

MpdTag *copyVorbisCommentBlockToMpdTag(const FLAC__StreamMetadata * block,
				       MpdTag * tag)
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
	DecoderControl *dc = data->dc;
	const FLAC__StreamMetadata_StreamInfo *si = &(block->data.stream_info);

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		dc->audioFormat.bits = si->bits_per_sample;
		dc->audioFormat.sampleRate = si->sample_rate;
		dc->audioFormat.channels = si->channels;
		dc->totalTime = ((float)si->total_samples) / (si->sample_rate);
		getOutputAudioFormat(&(dc->audioFormat),
				     &(data->cb->audioFormat));
		break;
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flacParseReplayGain(block, data);
	default:
		break;
	}
}

void flac_error_common_cb(const char *plugin,
			  const FLAC__StreamDecoderErrorStatus status,
			  FlacData * data)
{
	if (data->dc->stop)
		return;

	switch (status) {
	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
		ERROR("%s lost sync\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
		ERROR("bad %s header\n", plugin);
		break;
	case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
		ERROR("%s crc mismatch\n", plugin);
		break;
	default:
		ERROR("unknown %s error\n", plugin);
	}
}

#endif /* HAVE_FLAC || HAVE_OGGFLAC */
