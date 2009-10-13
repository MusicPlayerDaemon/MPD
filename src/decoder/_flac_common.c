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

/*
 * Common data structures and functions used by FLAC and OggFLAC
 */

#include "_flac_common.h"

#include <glib.h>

#include <assert.h>

void
flac_data_init(struct flac_data *data, struct decoder * decoder,
	       struct input_stream *input_stream)
{
	data->time = 0;
	data->position = 0;
	data->bit_rate = 0;
	data->decoder = decoder;
	data->input_stream = input_stream;
	data->replay_gain_info = NULL;
	data->tag = NULL;
}

static void
flac_find_float_comment(const FLAC__StreamMetadata *block,
			const char *cmnt, float *fl, bool *found_r)
{
	int offset;
	size_t pos;
	int len;
	unsigned char tmp, *p;

	offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block, 0,
								     cmnt);
	if (offset < 0)
		return;

	pos = strlen(cmnt) + 1; /* 1 is for '=' */
	len = block->data.vorbis_comment.comments[offset].length - pos;
	if (len <= 0)
		return;

	p = &block->data.vorbis_comment.comments[offset].entry[pos];
	tmp = p[len];
	p[len] = '\0';
	*fl = (float)atof((char *)p);
	p[len] = tmp;

	*found_r = true;
}

static void
flac_parse_replay_gain(const FLAC__StreamMetadata *block,
		       struct flac_data *data)
{
	bool found = false;

	if (data->replay_gain_info)
		replay_gain_info_free(data->replay_gain_info);

	data->replay_gain_info = replay_gain_info_new();

	flac_find_float_comment(block, "replaygain_album_gain",
				&data->replay_gain_info->tuples[REPLAY_GAIN_ALBUM].gain,
				&found);
	flac_find_float_comment(block, "replaygain_album_peak",
				&data->replay_gain_info->tuples[REPLAY_GAIN_ALBUM].peak,
				&found);
	flac_find_float_comment(block, "replaygain_track_gain",
				&data->replay_gain_info->tuples[REPLAY_GAIN_TRACK].gain,
				&found);
	flac_find_float_comment(block, "replaygain_track_peak",
				&data->replay_gain_info->tuples[REPLAY_GAIN_TRACK].peak,
				&found);

	if (!found) {
		replay_gain_info_free(data->replay_gain_info);
		data->replay_gain_info = NULL;
	}
}

/**
 * Checks if the specified name matches the entry's name, and if yes,
 * returns the comment value (not null-temrinated).
 */
static const char *
flac_comment_value(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		   const char *name, const char *char_tnum, size_t *length_r)
{
	size_t name_length = strlen(name);
	size_t char_tnum_length = 0;
	const char *comment = (const char*)entry->entry;

	if (entry->length <= name_length ||
	    g_ascii_strncasecmp(comment, name, name_length) != 0)
		return NULL;

	if (char_tnum != NULL) {
		char_tnum_length = strlen(char_tnum);
		if (entry->length > name_length + char_tnum_length + 2 &&
		    comment[name_length] == '[' &&
		    g_ascii_strncasecmp(comment + name_length + 1,
					char_tnum, char_tnum_length) == 0 &&
		    comment[name_length + char_tnum_length + 1] == ']')
			name_length = name_length + char_tnum_length + 2;
		else if (entry->length > name_length + char_tnum_length &&
			 g_ascii_strncasecmp(comment + name_length,
					     char_tnum, char_tnum_length) == 0)
			name_length = name_length + char_tnum_length;
	}

	if (comment[name_length] == '=') {
		*length_r = entry->length - name_length - 1;
		return comment + name_length + 1;
	}

	return NULL;
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
flac_copy_comment(struct tag *tag,
		  const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  const char *name, enum tag_type tag_type,
		  const char *char_tnum)
{
	const char *value;
	size_t value_length;

	value = flac_comment_value(entry, name, char_tnum, &value_length);
	if (value != NULL) {
		tag_add_item_n(tag, tag_type, value, value_length);
		return true;
	}

	return false;
}

/* tracknumber is used in VCs, MPD uses "track" ..., all the other
 * tag names match */
static const char *VORBIS_COMMENT_TRACK_KEY = "tracknumber";
static const char *VORBIS_COMMENT_DISC_KEY = "discnumber";

static void
flac_parse_comment(struct tag *tag, const char *char_tnum,
		   const FLAC__StreamMetadata_VorbisComment_Entry *entry)
{
	assert(tag != NULL);

	if (flac_copy_comment(tag, entry, VORBIS_COMMENT_TRACK_KEY,
			      TAG_TRACK, char_tnum) ||
	    flac_copy_comment(tag, entry, VORBIS_COMMENT_DISC_KEY,
			      TAG_DISC, char_tnum) ||
	    flac_copy_comment(tag, entry, "album artist",
			      TAG_ALBUM_ARTIST, char_tnum))
		return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (flac_copy_comment(tag, entry,
				      tag_item_names[i], i, char_tnum))
			return;
}

void
flac_vorbis_comments_to_tag(struct tag *tag, const char *char_tnum,
			    const FLAC__StreamMetadata *block)
{
	FLAC__StreamMetadata_VorbisComment_Entry *comments =
		block->data.vorbis_comment.comments;

	for (unsigned i = block->data.vorbis_comment.num_comments; i > 0; --i)
		flac_parse_comment(tag, char_tnum, comments++);
}

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     struct flac_data *data)
{
	const FLAC__StreamMetadata_StreamInfo *si = &(block->data.stream_info);

	switch (block->type) {
	case FLAC__METADATA_TYPE_STREAMINFO:
		audio_format_init(&data->audio_format, si->sample_rate,
				  si->bits_per_sample, si->channels);
		data->total_time = ((float)si->total_samples) / (si->sample_rate);
		break;
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flac_parse_replay_gain(block, data);

		if (data->tag != NULL)
			flac_vorbis_comments_to_tag(data->tag, NULL, block);

	default:
		break;
	}
}

void flac_error_common_cb(const char *plugin,
			  const FLAC__StreamDecoderErrorStatus status,
			  struct flac_data *data)
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
flac_common_write(struct flac_data *data, const FLAC__Frame * frame,
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

		cmd = decoder_data(data->decoder, data->input_stream,
				   data->chunk,
				   num_samples * bytes_per_channel,
				   data->time, data->bit_rate,
				   data->replay_gain_info);
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

#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7

char*
flac_cue_track(	const char* pathname,
		const unsigned int tnum)
{
	FLAC__bool success;
	FLAC__StreamMetadata* cs;

	success = FLAC__metadata_get_cuesheet(pathname, &cs);
	if (!success)
		return NULL;

	assert(cs != NULL);

	if (cs->data.cue_sheet.num_tracks <= 1)
	{
		FLAC__metadata_object_delete(cs);
		return NULL;
	}

	if (tnum > 0 && tnum < cs->data.cue_sheet.num_tracks)
	{
		char* track = g_strdup_printf("track_%03u.flac", tnum);

		FLAC__metadata_object_delete(cs);

		return track;
	}
	else
	{
		FLAC__metadata_object_delete(cs);
		return NULL;
	}
}

unsigned int
flac_vtrack_tnum(const char* fname)
{
	/* find last occurrence of '_' in fname
	 * which is hopefully something like track_xxx.flac
	 * another/better way would be to use tag struct
	 */
	char* ptr = strrchr(fname, '_');

	// copy ascii tracknumber to int
	char vtrack[4];
	g_strlcpy(vtrack, ++ptr, 4);
	return (unsigned int)strtol(vtrack, NULL, 10);
}

#endif /* FLAC_API_VERSION_CURRENT >= 7 */
