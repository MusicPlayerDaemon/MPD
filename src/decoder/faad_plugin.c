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

#include "../decoder_api.h"
#include "config.h"

#define AAC_MAX_CHANNELS	6

#include <assert.h>
#include <unistd.h>
#include <faad.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "faad"

/* all code here is either based on or copied from FAAD2's frontend code */
struct faad_buffer {
	struct decoder *decoder;
	struct input_stream *is;
	size_t length;
	size_t consumed;
	unsigned char data[FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS];
};

static void
faad_buffer_shift(struct faad_buffer *b, size_t length)
{
	assert(length >= b->consumed);
	assert(length <= b->consumed + b->length);

	memmove(b->data, b->data + length,
		b->consumed + b->length - length);

	length -= b->consumed;
	b->consumed = 0;
	b->length -= length;
}

static void
faad_buffer_fill(struct faad_buffer *b)
{
	size_t rest, bread;

	if (b->consumed > 0)
		faad_buffer_shift(b, b->consumed);

	rest = sizeof(b->data) - b->length;
	if (rest == 0)
		/* buffer already full */
		return;

	bread = decoder_read(b->decoder, b->is,
			     b->data + b->length, rest);
	b->length += bread;

	if ((b->length > 3 && memcmp(b->data, "TAG", 3) == 0) ||
	    (b->length > 11 &&
	     memcmp(b->data, "LYRICSBEGIN", 11) == 0) ||
	    (b->length > 8 && memcmp(b->data, "APETAGEX", 8) == 0))
		b->length = 0;
}

static void
faad_buffer_consume(struct faad_buffer *b, size_t bytes)
{
	b->consumed = bytes;
	b->length -= bytes;
}

static const unsigned adts_sample_rates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

/**
 * Check whether the buffer head is an AAC frame, and return the frame
 * length.  Returns 0 if it is not a frame.
 */
static size_t
adts_check_frame(struct faad_buffer *b)
{
	assert(b->length >= 8);

	/* check syncword */
	if (!((b->data[0] == 0xFF) && ((b->data[1] & 0xF6) == 0xF0)))
		return 0;

	return (((unsigned int)b->data[3] & 0x3) << 11) |
		(((unsigned int)b->data[4]) << 3) |
		(b->data[5] >> 5);
}

/**
 * Find the next AAC frame in the buffer.  Returns 0 if no frame is
 * found or if not enough data is available.
 */
static size_t
adts_find_frame(struct faad_buffer *b)
{
	const unsigned char *p;
	size_t frame_length;

	faad_buffer_fill(b);

	while ((p = memchr(b->data, 0xff, b->length)) != NULL) {
		/* discard data before 0xff */
		if (p > b->data)
			faad_buffer_shift(b, p - b->data);

		if (b->length < 8)
			/* not enough data yet */
			return 0;

		/* is it a frame? */
		frame_length = adts_check_frame(b);
		if (frame_length > 0)
			/* yes, it is */
			return frame_length;

		/* it's just some random 0xff byte; discard and and
		   continue searching */
		faad_buffer_shift(b, 1);
	}

	/* nothing at all; discard the whole buffer */
	faad_buffer_shift(b, b->length);
	return 0;
}

static float
adts_song_duration(struct faad_buffer *b)
{
	unsigned int frames, frame_length;
	unsigned sample_rate = 0;
	float frames_per_second;

	/* Read all frames to ensure correct time and bitrate */
	for (frames = 0;; frames++) {
		frame_length = adts_find_frame(b);
		if (frame_length > 0) {
			if (frames == 0) {
				sample_rate = adts_sample_rates[(b->data[2] & 0x3c) >> 2];
			}

			if (frame_length > b->length)
				break;

			faad_buffer_consume(b, frame_length);
		} else
			break;
	}

	frames_per_second = (float)sample_rate / 1024.0;
	if (frames_per_second <= 0)
		return -1;

	return (float)frames / frames_per_second;
}

static void
faad_buffer_init(struct faad_buffer *buffer, struct decoder *decoder,
		 struct input_stream *is)
{
	memset(buffer, 0, sizeof(*buffer));

	buffer->decoder = decoder;
	buffer->is = is;
}

static float
faad_song_duration(struct faad_buffer *b)
{
	size_t fileread;
	size_t tagsize;

	fileread = b->is->size >= 0 ? b->is->size : 0;

	faad_buffer_fill(b);

	tagsize = 0;
	if (b->length >= 10 && !memcmp(b->data, "ID3", 3)) {
		tagsize = (b->data[6] << 21) | (b->data[7] << 14) |
		    (b->data[8] << 7) | (b->data[9] << 0);

		tagsize += 10;
		faad_buffer_consume(b, tagsize);
		faad_buffer_fill(b);
	}

	if (b->is->seekable && b->length >= 2 &&
	    (b->data[0] == 0xFF) && ((b->data[1] & 0xF6) == 0xF0)) {
		float length = adts_song_duration(b);
		input_stream_seek(b->is, tagsize, SEEK_SET);

		b->length = 0;
		b->consumed = 0;

		faad_buffer_fill(b);

		return length;
	} else if (b->length >= 5 && memcmp(b->data, "ADIF", 4) == 0) {
		unsigned bit_rate;
		size_t skip_size = (b->data[4] & 0x80) ? 9 : 0;


		if (8 + skip_size > b->length)
			/* not enough data yet; skip parsing this
			   header */
			return -1;

		bit_rate = ((unsigned)(b->data[4 + skip_size] & 0x0F) << 19) |
			((unsigned)b->data[5 + skip_size] << 11) |
			((unsigned)b->data[6 + skip_size] << 3) |
			((unsigned)b->data[7 + skip_size] & 0xE0);

		if (fileread != 0 && bit_rate != 0)
			return fileread * 8.0 / bit_rate;
		else
			return fileread;
	} else
		return -1;
}

/**
 * Wrapper for faacDecInit() which works around some API
 * inconsistencies in libfaad.
 */
static bool
faad_decoder_init(faacDecHandle decoder, struct faad_buffer *buffer,
		  uint32_t *sample_rate, uint8_t *channels)
{
	int32_t nbytes;
#ifdef HAVE_FAAD_LONG
	/* neaacdec.h declares all arguments as "unsigned long", but
	   internally expects uint32_t pointers.  To avoid gcc
	   warnings, use this workaround. */
	unsigned long *sample_rate_r = (unsigned long *)(void *)sample_rate;
#else
	uint32_t *sample_rate_r = sample_rate;
#endif

	nbytes = faacDecInit(decoder, buffer->data,
#ifdef HAVE_FAAD_BUFLEN_FUNCS
			     buffer->length,
#endif
			     sample_rate_r, channels);
	if (nbytes < 0)
		return false;

	faad_buffer_consume(buffer, nbytes);
	return true;
}

/**
 * Wrapper for faacDecDecode() which works around some API
 * inconsistencies in libfaad.
 */
static const void *
faad_decoder_decode(faacDecHandle decoder, struct faad_buffer *buffer,
		    NeAACDecFrameInfo *frame_info)
{
	void *result;

	result = faacDecDecode(decoder, frame_info,
			       buffer->data
#ifdef HAVE_FAAD_BUFLEN_FUNCS
			       , buffer->length
#endif
			       );

	return result;
}

static float
faad_get_file_time_float(const char *file)
{
	struct faad_buffer buffer;
	float length;
	faacDecHandle decoder;
	faacDecConfigurationPtr config;
	uint32_t sample_rate;
	unsigned char channels;
	struct input_stream is;

	if (!input_stream_open(&is, file))
		return -1;

	faad_buffer_init(&buffer, NULL, &is);
	length = faad_song_duration(&buffer);

	if (length < 0) {
		bool ret;

		decoder = faacDecOpen();

		config = faacDecGetCurrentConfiguration(decoder);
		config->outputFormat = FAAD_FMT_16BIT;
		faacDecSetConfiguration(decoder, config);

		faad_buffer_fill(&buffer);

		ret = faad_decoder_init(decoder, &buffer,
					&sample_rate, &channels);
		if (ret && sample_rate > 0 && channels > 0)
			length = 0;

		faacDecClose(decoder);
	}

	input_stream_close(&is);

	return length;
}

static int
faad_get_file_time(const char *file)
{
	int file_time = -1;
	float length;

	if ((length = faad_get_file_time_float(file)) >= 0)
		file_time = length + 0.5;

	return file_time;
}

static void
faad_stream_decode(struct decoder *mpd_decoder, struct input_stream *is)
{
	float file_time;
	float total_time = 0;
	faacDecHandle decoder;
	faacDecFrameInfo frame_info;
	faacDecConfigurationPtr config;
	uint32_t sample_rate;
	unsigned char channels;
	unsigned long sample_count;
	bool ret;
	const void *decoded;
	size_t decoded_length;
	uint16_t bit_rate = 0;
	struct faad_buffer buffer;
	bool initialized = false;
	enum decoder_command cmd;

	faad_buffer_init(&buffer, mpd_decoder, is);
	total_time = faad_song_duration(&buffer);

	decoder = faacDecOpen();

	config = faacDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
#ifdef HAVE_FAACDECCONFIGURATION_DOWNMATRIX
	config->downMatrix = 1;
#endif
#ifdef HAVE_FAACDECCONFIGURATION_DONTUPSAMPLEIMPLICITSBR
	config->dontUpSampleImplicitSBR = 0;
#endif
	faacDecSetConfiguration(decoder, config);

	while (buffer.length < sizeof(buffer.data) &&
	       !input_stream_eof(buffer.is) &&
	       decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE) {
		adts_find_frame(&buffer);
		faad_buffer_fill(&buffer);
	}

	ret = faad_decoder_init(decoder, &buffer,
				&sample_rate, &channels);
	if (!ret) {
		g_warning("Error not a AAC stream.\n");
		faacDecClose(decoder);
		return;
	}

	file_time = 0.0;

	do {
		adts_find_frame(&buffer);
		faad_buffer_fill(&buffer);

		if (buffer.length == 0)
			break;

		decoded = faad_decoder_decode(decoder, &buffer, &frame_info);

		if (frame_info.error > 0) {
			g_warning("error decoding AAC stream: %s\n",
				  faacDecGetErrorMessage(frame_info.error));
			break;
		}
#ifdef HAVE_FAACDECFRAMEINFO_SAMPLERATE
		sample_rate = frame_info.samplerate;
#endif

		if (!initialized) {
			const struct audio_format audio_format = {
				.bits = 16,
				.channels = frame_info.channels,
				.sample_rate = sample_rate,
			};

			if (!audio_format_valid(&audio_format)) {
				g_warning("invalid audio format\n");
				break;
			}

			decoder_initialized(mpd_decoder, &audio_format,
					    false, total_time);
			initialized = true;
		}

		faad_buffer_consume(&buffer, frame_info.bytesconsumed);

		sample_count = (unsigned long)frame_info.samples;
		if (sample_count > 0) {
			bit_rate = frame_info.bytesconsumed * 8.0 *
			    frame_info.channels * sample_rate /
			    frame_info.samples / 1000 + 0.5;
			file_time +=
			    (float)(frame_info.samples) / frame_info.channels /
			    sample_rate;
		}

		decoded_length = sample_count * 2;

		cmd = decoder_data(mpd_decoder, is, decoded,
				   decoded_length, file_time,
				   bit_rate, NULL);
		if (cmd == DECODE_COMMAND_SEEK)
			decoder_seek_error(mpd_decoder);
	} while (cmd != DECODE_COMMAND_STOP);

	faacDecClose(decoder);
}

static struct tag *
faad_tag_dup(const char *file)
{
	int file_time = faad_get_file_time(file);
	struct tag *tag;

	if (file_time < 0) {
		g_debug("Failed to get total song time from: %s", file);
		return NULL;
	}

	tag = tag_new();
	tag->time = file_time;
	return tag;
}

static const char *const faad_suffixes[] = { "aac", NULL };
static const char *const faad_mime_types[] = {
	"audio/aac", "audio/aacp", NULL
};

const struct decoder_plugin faad_decoder_plugin = {
	.name = "faad",
	.stream_decode = faad_stream_decode,
	.tag_dup = faad_tag_dup,
	.suffixes = faad_suffixes,
	.mime_types = faad_mime_types,
};
