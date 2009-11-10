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
#include "conf.h"
#include "tag_id3.h"
#include "audio_check.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <mad.h>

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mad"

#define FRAMES_CUSHION    2000

#define READ_BUFFER_SIZE  40960

enum mp3_action {
	DECODE_SKIP = -3,
	DECODE_BREAK = -2,
	DECODE_CONT = -1,
	DECODE_OK = 0
};

enum muteframe {
	MUTEFRAME_NONE,
	MUTEFRAME_SKIP,
	MUTEFRAME_SEEK
};

/* the number of samples of silence the decoder inserts at start */
#define DECODERDELAY 529

#define DEFAULT_GAPLESS_MP3_PLAYBACK true

static bool gapless_playback;

static inline int32_t
mad_fixed_to_24_sample(mad_fixed_t sample)
{
	enum {
		bits = 24,
		MIN = -MAD_F_ONE,
		MAX = MAD_F_ONE - 1
	};

	/* round */
	sample = sample + (1L << (MAD_F_FRACBITS - bits));

	/* clip */
	if (sample > MAX)
		sample = MAX;
	else if (sample < MIN)
		sample = MIN;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - bits);
}

static void
mad_fixed_to_24_buffer(int32_t *dest, const struct mad_synth *synth,
		       unsigned int start, unsigned int end,
		       unsigned int num_channels)
{
	unsigned int i, c;

	for (i = start; i < end; ++i) {
		for (c = 0; c < num_channels; ++c)
			*dest++ = mad_fixed_to_24_sample(synth->pcm.samples[c][i]);
	}
}

static bool
mp3_plugin_init(G_GNUC_UNUSED const struct config_param *param)
{
	gapless_playback = config_get_bool(CONF_GAPLESS_MP3_PLAYBACK,
					   DEFAULT_GAPLESS_MP3_PLAYBACK);
	return true;
}

#define MP3_DATA_OUTPUT_BUFFER_SIZE 2048

struct mp3_data {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	mad_timer_t timer;
	unsigned char input_buffer[READ_BUFFER_SIZE];
	int32_t output_buffer[MP3_DATA_OUTPUT_BUFFER_SIZE];
	float total_time;
	float elapsed_time;
	float seek_where;
	enum muteframe mute_frame;
	long *frame_offsets;
	mad_timer_t *times;
	unsigned long highest_frame;
	unsigned long max_frames;
	unsigned long current_frame;
	unsigned int drop_start_frames;
	unsigned int drop_end_frames;
	unsigned int drop_start_samples;
	unsigned int drop_end_samples;
	bool found_xing;
	bool found_first_frame;
	bool decoded_first_frame;
	unsigned long bit_rate;
	struct decoder *decoder;
	struct input_stream *input_stream;
	enum mad_layer layer;
};

static void
mp3_data_init(struct mp3_data *data, struct decoder *decoder,
	      struct input_stream *input_stream)
{
	data->mute_frame = MUTEFRAME_NONE;
	data->highest_frame = 0;
	data->max_frames = 0;
	data->frame_offsets = NULL;
	data->times = NULL;
	data->current_frame = 0;
	data->drop_start_frames = 0;
	data->drop_end_frames = 0;
	data->drop_start_samples = 0;
	data->drop_end_samples = 0;
	data->found_xing = false;
	data->found_first_frame = false;
	data->decoded_first_frame = false;
	data->decoder = decoder;
	data->input_stream = input_stream;
	data->layer = 0;

	mad_stream_init(&data->stream);
	mad_stream_options(&data->stream, MAD_OPTION_IGNORECRC);
	mad_frame_init(&data->frame);
	mad_synth_init(&data->synth);
	mad_timer_reset(&data->timer);
}

static bool mp3_seek(struct mp3_data *data, long offset)
{
	if (!input_stream_seek(data->input_stream, offset, SEEK_SET))
		return false;

	mad_stream_buffer(&data->stream, data->input_buffer, 0);
	(data->stream).error = 0;

	return true;
}

static bool
mp3_fill_buffer(struct mp3_data *data)
{
	size_t remaining, length;
	unsigned char *dest;

	if (data->stream.next_frame != NULL) {
		remaining = data->stream.bufend - data->stream.next_frame;
		memmove(data->input_buffer, data->stream.next_frame,
			remaining);
		dest = (data->input_buffer) + remaining;
		length = READ_BUFFER_SIZE - remaining;
	} else {
		remaining = 0;
		length = READ_BUFFER_SIZE;
		dest = data->input_buffer;
	}

	/* we've exhausted the read buffer, so give up!, these potential
	 * mp3 frames are way too big, and thus unlikely to be mp3 frames */
	if (length == 0)
		return false;

	length = decoder_read(data->decoder, data->input_stream, dest, length);
	if (length == 0)
		return false;

	mad_stream_buffer(&data->stream, data->input_buffer,
			  length + remaining);
	(data->stream).error = 0;

	return true;
}

#ifdef HAVE_ID3TAG
/* Parse mp3 RVA2 frame. Shamelessly stolen from madplay. */
static int parse_rva2(struct id3_tag * tag, struct replay_gain_info * replay_gain_info)
{
	struct id3_frame const * frame;

	id3_latin1_t const *id;
	id3_byte_t const *data;
	id3_length_t length;
	int found;

	enum {
		CHANNEL_OTHER         = 0x00,
		CHANNEL_MASTER_VOLUME = 0x01,
		CHANNEL_FRONT_RIGHT   = 0x02,
		CHANNEL_FRONT_LEFT    = 0x03,
		CHANNEL_BACK_RIGHT    = 0x04,
		CHANNEL_BACK_LEFT     = 0x05,
		CHANNEL_FRONT_CENTRE  = 0x06,
		CHANNEL_BACK_CENTRE   = 0x07,
		CHANNEL_SUBWOOFER     = 0x08
	};

	found = 0;

	/* relative volume adjustment information */

	frame = id3_tag_findframe(tag, "RVA2", 0);
	if (!frame) return 0;

	id   = id3_field_getlatin1(id3_frame_field(frame, 0));
	data = id3_field_getbinarydata(id3_frame_field(frame, 1),
					&length);

	if (!id || !data) return 0;

	/*
	 * "The 'identification' string is used to identify the
	 * situation and/or device where this adjustment should apply.
	 * The following is then repeated for every channel
	 *
	 *   Type of channel         $xx
	 *   Volume adjustment       $xx xx
	 *   Bits representing peak  $xx
	 *   Peak volume             $xx (xx ...)"
	 */

	while (length >= 4) {
		unsigned int peak_bytes;

		peak_bytes = (data[3] + 7) / 8;
		if (4 + peak_bytes > length)
			break;

		if (data[0] == CHANNEL_MASTER_VOLUME) {
			signed int voladj_fixed;
			double voladj_float;

			/*
			 * "The volume adjustment is encoded as a fixed
			 * point decibel value, 16 bit signed integer
			 * representing (adjustment*512), giving +/- 64
			 * dB with a precision of 0.001953125 dB."
			 */

			voladj_fixed  = (data[1] << 8) | (data[2] << 0);
			voladj_fixed |= -(voladj_fixed & 0x8000);

			voladj_float  = (double) voladj_fixed / 512;

			replay_gain_info->tuples[REPLAY_GAIN_TRACK].peak = voladj_float;
			replay_gain_info->tuples[REPLAY_GAIN_ALBUM].peak = voladj_float;

			g_debug("parseRVA2: Relative Volume "
				"%+.1f dB adjustment (%s)\n",
				voladj_float, id);

			found = 1;
			break;
		}

		data   += 4 + peak_bytes;
		length -= 4 + peak_bytes;
	}

	return found;
}
#endif

#ifdef HAVE_ID3TAG
static struct replay_gain_info *
parse_id3_replay_gain_info(struct id3_tag *tag)
{
	int i;
	char *key;
	char *value;
	struct id3_frame *frame;
	bool found = false;
	struct replay_gain_info *replay_gain_info;

	replay_gain_info = replay_gain_info_new();

	for (i = 0; (frame = id3_tag_findframe(tag, "TXXX", i)); i++) {
		if (frame->nfields < 3)
			continue;

		key = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[1]));
		value = (char *)
		    id3_ucs4_latin1duplicate(id3_field_getstring
					     (&frame->fields[2]));

		if (g_ascii_strcasecmp(key, "replaygain_track_gain") == 0) {
			replay_gain_info->tuples[REPLAY_GAIN_TRACK].gain = atof(value);
			found = true;
		} else if (g_ascii_strcasecmp(key, "replaygain_album_gain") == 0) {
			replay_gain_info->tuples[REPLAY_GAIN_ALBUM].gain = atof(value);
			found = true;
		} else if (g_ascii_strcasecmp(key, "replaygain_track_peak") == 0) {
			replay_gain_info->tuples[REPLAY_GAIN_TRACK].peak = atof(value);
			found = true;
		} else if (g_ascii_strcasecmp(key, "replaygain_album_peak") == 0) {
			replay_gain_info->tuples[REPLAY_GAIN_ALBUM].peak = atof(value);
			found = true;
		}

		free(key);
		free(value);
	}

	if (!found) {
		/* fall back on RVA2 if no replaygain tags found */
		found = parse_rva2(tag, replay_gain_info);
	}

	if (found)
		return replay_gain_info;
	replay_gain_info_free(replay_gain_info);
	return NULL;
}
#endif

static void mp3_parse_id3(struct mp3_data *data, size_t tagsize,
			  struct tag **mpd_tag,
			  struct replay_gain_info **replay_gain_info_r)
{
#ifdef HAVE_ID3TAG
	struct id3_tag *id3_tag = NULL;
	id3_length_t count;
	id3_byte_t const *id3_data;
	id3_byte_t *allocated = NULL;

	count = data->stream.bufend - data->stream.this_frame;

	if (tagsize <= count) {
		id3_data = data->stream.this_frame;
		mad_stream_skip(&(data->stream), tagsize);
	} else {
		allocated = g_malloc(tagsize);
		memcpy(allocated, data->stream.this_frame, count);
		mad_stream_skip(&(data->stream), count);

		while (count < tagsize) {
			size_t len;

			len = decoder_read(data->decoder, data->input_stream,
					   allocated + count, tagsize - count);
			if (len == 0)
				break;
			else
				count += len;
		}

		if (count != tagsize) {
			g_debug("error parsing ID3 tag");
			g_free(allocated);
			return;
		}

		id3_data = allocated;
	}

	id3_tag = id3_tag_parse(id3_data, tagsize);
	if (id3_tag == NULL) {
		g_free(allocated);
		return;
	}

	if (mpd_tag) {
		struct tag *tmp_tag = tag_id3_import(id3_tag);
		if (tmp_tag != NULL) {
			if (*mpd_tag != NULL)
				tag_free(*mpd_tag);
			*mpd_tag = tmp_tag;
		}
	}

	if (replay_gain_info_r) {
		struct replay_gain_info *tmp_rgi =
			parse_id3_replay_gain_info(id3_tag);
		if (tmp_rgi != NULL) {
			if (*replay_gain_info_r)
				replay_gain_info_free(*replay_gain_info_r);
			*replay_gain_info_r = tmp_rgi;
		}
	}

	id3_tag_delete(id3_tag);

	g_free(allocated);
#else /* !HAVE_ID3TAG */
	(void)mpd_tag;
	(void)replay_gain_info_r;

	/* This code is enabled when libid3tag is disabled.  Instead
	   of parsing the ID3 frame, it just skips it. */

	mad_stream_skip(&data->stream, tagsize);
#endif
}

#ifndef HAVE_ID3TAG
/**
 * This function emulates libid3tag when it is disabled.  Instead of
 * doing a real analyzation of the frame, it just checks whether the
 * frame begins with the string "ID3".  If so, it returns the full
 * length.
 */
static signed long
id3_tag_query(const void *p0, size_t length)
{
	const char *p = p0;

	return length > 3 && memcmp(p, "ID3", 3) == 0
		? length
		: 0;
}
#endif /* !HAVE_ID3TAG */

static enum mp3_action
decode_next_frame_header(struct mp3_data *data, G_GNUC_UNUSED struct tag **tag,
			 G_GNUC_UNUSED struct replay_gain_info **replay_gain_info_r)
{
	enum mad_layer layer;

	if ((data->stream).buffer == NULL
	    || (data->stream).error == MAD_ERROR_BUFLEN) {
		if (!mp3_fill_buffer(data))
			return DECODE_BREAK;
	}
	if (mad_header_decode(&data->frame.header, &data->stream)) {
		if ((data->stream).error == MAD_ERROR_LOSTSYNC &&
		    (data->stream).this_frame) {
			signed long tagsize = id3_tag_query((data->stream).
							    this_frame,
							    (data->stream).
							    bufend -
							    (data->stream).
							    this_frame);

			if (tagsize > 0) {
				if (tag && !(*tag)) {
					mp3_parse_id3(data, (size_t)tagsize,
						      tag, replay_gain_info_r);
				} else {
					mad_stream_skip(&(data->stream),
							tagsize);
				}
				return DECODE_CONT;
			}
		}
		if (MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		} else {
			if ((data->stream).error == MAD_ERROR_BUFLEN)
				return DECODE_CONT;
			else {
				g_warning("unrecoverable frame level error "
					  "(%s).\n",
					  mad_stream_errorstr(&data->stream));
				return DECODE_BREAK;
			}
		}
	}

	layer = data->frame.header.layer;
	if (!data->layer) {
		if (layer != MAD_LAYER_II && layer != MAD_LAYER_III) {
			/* Only layer 2 and 3 have been tested to work */
			return DECODE_SKIP;
		}
		data->layer = layer;
	} else if (layer != data->layer) {
		/* Don't decode frames with a different layer than the first */
		return DECODE_SKIP;
	}

	return DECODE_OK;
}

static enum mp3_action
decodeNextFrame(struct mp3_data *data)
{
	if ((data->stream).buffer == NULL
	    || (data->stream).error == MAD_ERROR_BUFLEN) {
		if (!mp3_fill_buffer(data))
			return DECODE_BREAK;
	}
	if (mad_frame_decode(&data->frame, &data->stream)) {
		if ((data->stream).error == MAD_ERROR_LOSTSYNC) {
			signed long tagsize = id3_tag_query((data->stream).
							    this_frame,
							    (data->stream).
							    bufend -
							    (data->stream).
							    this_frame);
			if (tagsize > 0) {
				mad_stream_skip(&(data->stream), tagsize);
				return DECODE_CONT;
			}
		}
		if (MAD_RECOVERABLE((data->stream).error)) {
			return DECODE_SKIP;
		} else {
			if ((data->stream).error == MAD_ERROR_BUFLEN)
				return DECODE_CONT;
			else {
				g_warning("unrecoverable frame level error "
					  "(%s).\n",
					  mad_stream_errorstr(&data->stream));
				return DECODE_BREAK;
			}
		}
	}

	return DECODE_OK;
}

/* xing stuff stolen from alsaplayer, and heavily modified by jat */
#define XI_MAGIC (('X' << 8) | 'i')
#define NG_MAGIC (('n' << 8) | 'g')
#define IN_MAGIC (('I' << 8) | 'n')
#define FO_MAGIC (('f' << 8) | 'o')

enum xing_magic {
	XING_MAGIC_XING, /* VBR */
	XING_MAGIC_INFO  /* CBR */
};

struct xing {
	long flags;             /* valid fields (see below) */
	unsigned long frames;   /* total number of frames */
	unsigned long bytes;    /* total number of bytes */
	unsigned char toc[100]; /* 100-point seek table */
	long scale;             /* VBR quality */
	enum xing_magic magic;  /* header magic */
};

enum {
	XING_FRAMES = 0x00000001L,
	XING_BYTES  = 0x00000002L,
	XING_TOC    = 0x00000004L,
	XING_SCALE  = 0x00000008L
};

struct version {
	unsigned major;
	unsigned minor;
};

struct lame {
	char encoder[10];       /* 9 byte encoder name/version ("LAME3.97b") */
	struct version version; /* struct containing just the version */
	float peak;             /* replaygain peak */
	float track_gain;       /* replaygain track gain */
	float album_gain;       /* replaygain album gain */
	int encoder_delay;      /* # of added samples at start of mp3 */
	int encoder_padding;    /* # of added samples at end of mp3 */
	int crc;                /* CRC of the first 190 bytes of this frame */
};

static bool
parse_xing(struct xing *xing, struct mad_bitptr *ptr, int *oldbitlen)
{
	unsigned long bits;
	int bitlen;
	int bitsleft;
	int i;

	bitlen = *oldbitlen;

	if (bitlen < 16)
		return false;

	bits = mad_bit_read(ptr, 16);
	bitlen -= 16;

	if (bits == XI_MAGIC) {
		if (bitlen < 16)
			return false;

		if (mad_bit_read(ptr, 16) != NG_MAGIC)
			return false;

		bitlen -= 16;
		xing->magic = XING_MAGIC_XING;
	} else if (bits == IN_MAGIC) {
		if (bitlen < 16)
			return false;

		if (mad_bit_read(ptr, 16) != FO_MAGIC)
			return false;

		bitlen -= 16;
		xing->magic = XING_MAGIC_INFO;
	}
	else if (bits == NG_MAGIC) xing->magic = XING_MAGIC_XING;
	else if (bits == FO_MAGIC) xing->magic = XING_MAGIC_INFO;
	else
		return false;

	if (bitlen < 32)
		return false;
	xing->flags = mad_bit_read(ptr, 32);
	bitlen -= 32;

	if (xing->flags & XING_FRAMES) {
		if (bitlen < 32)
			return false;
		xing->frames = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_BYTES) {
		if (bitlen < 32)
			return false;
		xing->bytes = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	if (xing->flags & XING_TOC) {
		if (bitlen < 800)
			return false;
		for (i = 0; i < 100; ++i) xing->toc[i] = mad_bit_read(ptr, 8);
		bitlen -= 800;
	}

	if (xing->flags & XING_SCALE) {
		if (bitlen < 32)
			return false;
		xing->scale = mad_bit_read(ptr, 32);
		bitlen -= 32;
	}

	/* Make sure we consume no less than 120 bytes (960 bits) in hopes that
	 * the LAME tag is found there, and not right after the Xing header */
	bitsleft = 960 - ((*oldbitlen) - bitlen);
	if (bitsleft < 0)
		return false;
	else if (bitsleft > 0) {
		mad_bit_read(ptr, bitsleft);
		bitlen -= bitsleft;
	}

	*oldbitlen = bitlen;

	return true;
}

static bool
parse_lame(struct lame *lame, struct mad_bitptr *ptr, int *bitlen)
{
	int adj = 0;
	int name;
	int orig;
	int sign;
	int gain;
	int i;

	/* Unlike the xing header, the lame tag has a fixed length.  Fail if
	 * not all 36 bytes (288 bits) are there. */
	if (*bitlen < 288)
		return false;

	for (i = 0; i < 9; i++)
		lame->encoder[i] = (char)mad_bit_read(ptr, 8);
	lame->encoder[9] = '\0';

	*bitlen -= 72;

	/* This is technically incorrect, since the encoder might not be lame.
	 * But there's no other way to determine if this is a lame tag, and we
	 * wouldn't want to go reading a tag that's not there. */
	if (!g_str_has_prefix(lame->encoder, "LAME"))
		return false;

	if (sscanf(lame->encoder+4, "%u.%u",
	           &lame->version.major, &lame->version.minor) != 2)
		return false;

	g_debug("detected LAME version %i.%i (\"%s\")\n",
		lame->version.major, lame->version.minor, lame->encoder);

	/* The reference volume was changed from the 83dB used in the
	 * ReplayGain spec to 89dB in lame 3.95.1.  Bump the gain for older
	 * versions, since everyone else uses 89dB instead of 83dB.
	 * Unfortunately, lame didn't differentiate between 3.95 and 3.95.1, so
	 * it's impossible to make the proper adjustment for 3.95.
	 * Fortunately, 3.95 was only out for about a day before 3.95.1 was
	 * released. -- tmz */
	if (lame->version.major < 3 ||
	    (lame->version.major == 3 && lame->version.minor < 95))
		adj = 6;

	mad_bit_read(ptr, 16);

	lame->peak = mad_f_todouble(mad_bit_read(ptr, 32) << 5); /* peak */
	g_debug("LAME peak found: %f\n", lame->peak);

	lame->track_gain = 0;
	name = mad_bit_read(ptr, 3); /* gain name */
	orig = mad_bit_read(ptr, 3); /* gain originator */
	sign = mad_bit_read(ptr, 1); /* sign bit */
	gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 1 && orig != 0) {
		lame->track_gain = ((sign ? -gain : gain) / 10.0) + adj;
		g_debug("LAME track gain found: %f\n", lame->track_gain);
	}

	/* tmz reports that this isn't currently written by any version of lame
	 * (as of 3.97).  Since we have no way of testing it, don't use it.
	 * Wouldn't want to go blowing someone's ears just because we read it
	 * wrong. :P -- jat */
	lame->album_gain = 0;
#if 0
	name = mad_bit_read(ptr, 3); /* gain name */
	orig = mad_bit_read(ptr, 3); /* gain originator */
	sign = mad_bit_read(ptr, 1); /* sign bit */
	gain = mad_bit_read(ptr, 9); /* gain*10 */
	if (gain && name == 2 && orig != 0) {
		lame->album_gain = ((sign ? -gain : gain) / 10.0) + adj;
		g_debug("LAME album gain found: %f\n", lame->track_gain);
	}
#else
	mad_bit_read(ptr, 16);
#endif

	mad_bit_read(ptr, 16);

	lame->encoder_delay = mad_bit_read(ptr, 12);
	lame->encoder_padding = mad_bit_read(ptr, 12);

	g_debug("encoder delay is %i, encoder padding is %i\n",
	      lame->encoder_delay, lame->encoder_padding);

	mad_bit_read(ptr, 80);

	lame->crc = mad_bit_read(ptr, 16);

	*bitlen -= 216;

	return true;
}

static inline float
mp3_frame_duration(const struct mad_frame *frame)
{
	return mad_timer_count(frame->header.duration,
			       MAD_UNITS_MILLISECONDS) / 1000.0;
}

static goffset
mp3_this_frame_offset(const struct mp3_data *data)
{
	goffset offset = data->input_stream->offset;

	if (data->stream.this_frame != NULL)
		offset -= data->stream.bufend - data->stream.this_frame;
	else
		offset -= data->stream.bufend - data->stream.buffer;

	return offset;
}

static goffset
mp3_rest_including_this_frame(const struct mp3_data *data)
{
	return data->input_stream->size - mp3_this_frame_offset(data);
}

/**
 * Attempt to calulcate the length of the song from filesize
 */
static void
mp3_filesize_to_song_length(struct mp3_data *data)
{
	goffset rest = mp3_rest_including_this_frame(data);

	if (rest > 0) {
		float frame_duration = mp3_frame_duration(&data->frame);

		data->total_time = (rest * 8.0) / (data->frame).header.bitrate;
		data->max_frames = data->total_time / frame_duration +
			FRAMES_CUSHION;
	} else {
		data->max_frames = FRAMES_CUSHION;
		data->total_time = 0;
	}
}

static bool
mp3_decode_first_frame(struct mp3_data *data, struct tag **tag,
		       struct replay_gain_info **replay_gain_info_r)
{
	struct xing xing;
	struct lame lame;
	struct mad_bitptr ptr;
	int bitlen;
	enum mp3_action ret;

	/* stfu gcc */
	memset(&xing, 0, sizeof(struct xing));
	xing.flags = 0;

	while (true) {
		do {
			ret = decode_next_frame_header(data, tag,
						       replay_gain_info_r);
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;
		if (ret == DECODE_SKIP) continue;

		do {
			ret = decodeNextFrame(data);
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;
		if (ret == DECODE_OK) break;
	}

	ptr = data->stream.anc_ptr;
	bitlen = data->stream.anc_bitlen;

	mp3_filesize_to_song_length(data);

	/*
	 * if an xing tag exists, use that!
	 */
	if (parse_xing(&xing, &ptr, &bitlen)) {
		data->found_xing = true;
		data->mute_frame = MUTEFRAME_SKIP;

		if ((xing.flags & XING_FRAMES) && xing.frames) {
			mad_timer_t duration = data->frame.header.duration;
			mad_timer_multiply(&duration, xing.frames);
			data->total_time = ((float)mad_timer_count(duration, MAD_UNITS_MILLISECONDS)) / 1000;
			data->max_frames = xing.frames;
		}

		if (parse_lame(&lame, &ptr, &bitlen)) {
			if (gapless_playback &&
			    data->input_stream->seekable) {
				data->drop_start_samples = lame.encoder_delay +
				                           DECODERDELAY;
				data->drop_end_samples = lame.encoder_padding;
			}

			/* Album gain isn't currently used.  See comment in
			 * parse_lame() for details. -- jat */
			if (replay_gain_info_r && !*replay_gain_info_r &&
			    lame.track_gain) {
				*replay_gain_info_r = replay_gain_info_new();
				(*replay_gain_info_r)->tuples[REPLAY_GAIN_TRACK].gain = lame.track_gain;
				(*replay_gain_info_r)->tuples[REPLAY_GAIN_TRACK].peak = lame.peak;
			}
		}
	} 

	if (!data->max_frames)
		return false;

	if (data->max_frames > 8 * 1024 * 1024) {
		g_warning("mp3 file header indicates too many frames: %lu\n",
			  data->max_frames);
		return false;
	}

	data->frame_offsets = g_malloc(sizeof(long) * data->max_frames);
	data->times = g_malloc(sizeof(mad_timer_t) * data->max_frames);

	return true;
}

static void mp3_data_finish(struct mp3_data *data)
{
	mad_synth_finish(&data->synth);
	mad_frame_finish(&data->frame);
	mad_stream_finish(&data->stream);

	g_free(data->frame_offsets);
	g_free(data->times);
}

/* this is primarily used for getting total time for tags */
static int mp3_total_file_time(const char *file)
{
	struct input_stream input_stream;
	struct mp3_data data;
	int ret;

	if (!input_stream_open(&input_stream, file))
		return -1;
	mp3_data_init(&data, NULL, &input_stream);
	if (!mp3_decode_first_frame(&data, NULL, NULL))
		ret = -1;
	else
		ret = data.total_time + 0.5;
	mp3_data_finish(&data);
	input_stream_close(&input_stream);

	return ret;
}

static bool
mp3_open(struct input_stream *is, struct mp3_data *data,
	 struct decoder *decoder, struct tag **tag,
	 struct replay_gain_info **replay_gain_info_r)
{
	mp3_data_init(data, decoder, is);
	*tag = NULL;
	if (!mp3_decode_first_frame(data, tag, replay_gain_info_r)) {
		mp3_data_finish(data);
		if (tag && *tag)
			tag_free(*tag);
		return false;
	}

	return true;
}

static long
mp3_time_to_frame(const struct mp3_data *data, double t)
{
	unsigned long i;

	for (i = 0; i < data->highest_frame; ++i) {
		double frame_time =
			mad_timer_count(data->times[i],
					MAD_UNITS_MILLISECONDS) / 1000.;
		if (frame_time >= t)
			break;
	}

	return i;
}

static void
mp3_update_timer_next_frame(struct mp3_data *data)
{
	if (data->current_frame >= data->highest_frame) {
		/* record this frame's properties in
		   data->frame_offsets (for seeking) and
		   data->times */
		data->bit_rate = (data->frame).header.bitrate;

		if (data->current_frame >= data->max_frames)
			/* cap data->current_frame */
			data->current_frame = data->max_frames - 1;
		else
			data->highest_frame++;

		data->frame_offsets[data->current_frame] =
			mp3_this_frame_offset(data);

		mad_timer_add(&data->timer, (data->frame).header.duration);
		data->times[data->current_frame] = data->timer;
	} else
		/* get the new timer value from data->times */
		data->timer = data->times[data->current_frame];

	data->current_frame++;
	data->elapsed_time =
		mad_timer_count(data->timer, MAD_UNITS_MILLISECONDS) / 1000.0;
}

/**
 * Sends the synthesized current frame via decoder_data().
 */
static enum decoder_command
mp3_send_pcm(struct mp3_data *data, unsigned i, unsigned pcm_length,
	     struct replay_gain_info *replay_gain_info)
{
	unsigned max_samples;

	max_samples = sizeof(data->output_buffer) /
		sizeof(data->output_buffer[0]) /
		MAD_NCHANNELS(&(data->frame).header);

	while (i < pcm_length) {
		enum decoder_command cmd;
		unsigned int num_samples = pcm_length - i;
		if (num_samples > max_samples)
			num_samples = max_samples;

		i += num_samples;

		mad_fixed_to_24_buffer(data->output_buffer,
				       &data->synth,
				       i - num_samples, i,
				       MAD_NCHANNELS(&(data->frame).header));
		num_samples *= MAD_NCHANNELS(&(data->frame).header);

		cmd = decoder_data(data->decoder, data->input_stream,
				   data->output_buffer,
				   sizeof(data->output_buffer[0]) * num_samples,
				   data->elapsed_time,
				   data->bit_rate / 1000,
				   replay_gain_info);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	return DECODE_COMMAND_NONE;
}

/**
 * Synthesize the current frame and send it via decoder_data().
 */
static enum decoder_command
mp3_synth_and_send(struct mp3_data *data,
		   struct replay_gain_info *replay_gain_info)
{
	unsigned i, pcm_length;
	enum decoder_command cmd;

	mad_synth_frame(&data->synth, &data->frame);

	if (!data->found_first_frame) {
		unsigned int samples_per_frame = data->synth.pcm.length;
		data->drop_start_frames = data->drop_start_samples / samples_per_frame;
		data->drop_end_frames = data->drop_end_samples / samples_per_frame;
		data->drop_start_samples = data->drop_start_samples % samples_per_frame;
		data->drop_end_samples = data->drop_end_samples % samples_per_frame;
		data->found_first_frame = true;
	}

	if (data->drop_start_frames > 0) {
		data->drop_start_frames--;
		return DECODE_COMMAND_NONE;
	} else if ((data->drop_end_frames > 0) &&
		   (data->current_frame == (data->max_frames + 1 - data->drop_end_frames))) {
		/* stop decoding, effectively dropping all remaining
		   frames */
		return DECODE_COMMAND_STOP;
	}

	if (!data->decoded_first_frame) {
		i = data->drop_start_samples;
		data->decoded_first_frame = true;
	} else
		i = 0;

	pcm_length = data->synth.pcm.length;
	if (data->drop_end_samples &&
	    (data->current_frame == data->max_frames - data->drop_end_frames)) {
		if (data->drop_end_samples >= pcm_length)
			pcm_length = 0;
		else
			pcm_length -= data->drop_end_samples;
	}

	cmd = mp3_send_pcm(data, i, pcm_length, replay_gain_info);
	if (cmd != DECODE_COMMAND_NONE)
		return cmd;

	if (data->drop_end_samples &&
	    (data->current_frame == data->max_frames - data->drop_end_frames))
		/* stop decoding, effectively dropping
		 * all remaining samples */
		return DECODE_COMMAND_STOP;

	return DECODE_COMMAND_NONE;
}

static bool
mp3_read(struct mp3_data *data, struct replay_gain_info **replay_gain_info_r)
{
	struct decoder *decoder = data->decoder;
	enum mp3_action ret;
	enum decoder_command cmd;

	mp3_update_timer_next_frame(data);

	switch (data->mute_frame) {
	case MUTEFRAME_SKIP:
		data->mute_frame = MUTEFRAME_NONE;
		break;
	case MUTEFRAME_SEEK:
		if (data->elapsed_time >= data->seek_where)
			data->mute_frame = MUTEFRAME_NONE;
		break;
	case MUTEFRAME_NONE:
		cmd = mp3_synth_and_send(data,
					 replay_gain_info_r != NULL
					 ? *replay_gain_info_r : NULL);
		if (cmd == DECODE_COMMAND_SEEK) {
			unsigned long j;

			assert(data->input_stream->seekable);

			j = mp3_time_to_frame(data,
					      decoder_seek_where(decoder));
			if (j < data->highest_frame) {
				if (mp3_seek(data, data->frame_offsets[j])) {
					data->current_frame = j;
					decoder_command_finished(decoder);
				} else
					decoder_seek_error(decoder);
			} else {
				data->seek_where = decoder_seek_where(decoder);
				data->mute_frame = MUTEFRAME_SEEK;
				decoder_command_finished(decoder);
			}
		} else if (cmd != DECODE_COMMAND_NONE)
			return false;
	}

	while (true) {
		bool skip = false;

		do {
			struct tag *tag = NULL;

			ret = decode_next_frame_header(data, &tag,
						       replay_gain_info_r);

			if (tag != NULL) {
				decoder_tag(decoder, data->input_stream, tag);
				tag_free(tag);
			}
		} while (ret == DECODE_CONT);
		if (ret == DECODE_BREAK)
			return false;
		else if (ret == DECODE_SKIP)
			skip = true;

		if (data->mute_frame == MUTEFRAME_NONE) {
			do {
				ret = decodeNextFrame(data);
			} while (ret == DECODE_CONT);
			if (ret == DECODE_BREAK)
				return false;
		}

		if (!skip && ret == DECODE_OK)
			break;
	}

	return ret != DECODE_BREAK;
}

static void
mp3_decode(struct decoder *decoder, struct input_stream *input_stream)
{
	struct mp3_data data;
	GError *error = NULL;
	struct tag *tag = NULL;
	struct replay_gain_info *replay_gain_info = NULL;
	struct audio_format audio_format;

	if (!mp3_open(input_stream, &data, decoder, &tag, &replay_gain_info)) {
		if (decoder_get_command(decoder) == DECODE_COMMAND_NONE)
			g_warning
			    ("Input does not appear to be a mp3 bit stream.\n");
		return;
	}

	if (!audio_format_init_checked(&audio_format,
				       data.frame.header.samplerate, 24,
				       MAD_NCHANNELS(&data.frame.header),
				       &error)) {
		g_warning("%s", error->message);
		g_error_free(error);

		if (tag != NULL)
			tag_free(tag);
		if (replay_gain_info != NULL)
			replay_gain_info_free(replay_gain_info);
		mp3_data_finish(&data);
		return;
	}

	decoder_initialized(decoder, &audio_format,
			    data.input_stream->seekable, data.total_time);

	if (tag != NULL) {
		decoder_tag(decoder, input_stream, tag);
		tag_free(tag);
	}

	while (mp3_read(&data, &replay_gain_info)) ;

	if (replay_gain_info)
		replay_gain_info_free(replay_gain_info);

	if (decoder_get_command(decoder) == DECODE_COMMAND_SEEK &&
	    data.mute_frame == MUTEFRAME_SEEK)
		decoder_command_finished(decoder);

	mp3_data_finish(&data);
}

static struct tag *mp3_tag_dup(const char *file)
{
	struct tag *tag;
	int total_time;

	total_time = mp3_total_file_time(file);
	if (total_time < 0) {
		g_debug("Failed to get total song time from: %s", file);
		return NULL;
	}

	tag = tag_new();
	tag->time = total_time;
	return tag;
}

static const char *const mp3_suffixes[] = { "mp3", "mp2", NULL };
static const char *const mp3_mime_types[] = { "audio/mpeg", NULL };

const struct decoder_plugin mad_decoder_plugin = {
	.name = "mad",
	.init = mp3_plugin_init,
	.stream_decode = mp3_decode,
	.tag_dup = mp3_tag_dup,
	.suffixes = mp3_suffixes,
	.mime_types = mp3_mime_types
};
