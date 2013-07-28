/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FaadDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "DecoderBuffer.hxx"
#include "audio_check.h"
#include "tag_handler.h"

#include <neaacdec.h>

#include <glib.h>

#include <assert.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "faad"

#define AAC_MAX_CHANNELS	6

static const unsigned adts_sample_rates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

/**
 * The GLib quark used for errors reported by this plugin.
 */
static inline GQuark
faad_decoder_quark(void)
{
	return g_quark_from_static_string("faad");
}

/**
 * Check whether the buffer head is an AAC frame, and return the frame
 * length.  Returns 0 if it is not a frame.
 */
static size_t
adts_check_frame(const unsigned char *data)
{
	/* check syncword */
	if (!((data[0] == 0xFF) && ((data[1] & 0xF6) == 0xF0)))
		return 0;

	return (((unsigned int)data[3] & 0x3) << 11) |
		(((unsigned int)data[4]) << 3) |
		(data[5] >> 5);
}

/**
 * Find the next AAC frame in the buffer.  Returns 0 if no frame is
 * found or if not enough data is available.
 */
static size_t
adts_find_frame(DecoderBuffer *buffer)
{
	size_t length, frame_length;
	bool ret;

	while (true) {
		const uint8_t *data = (const uint8_t *)
			decoder_buffer_read(buffer, &length);
		if (data == nullptr || length < 8) {
			/* not enough data yet */
			ret = decoder_buffer_fill(buffer);
			if (!ret)
				/* failed */
				return 0;

			continue;
		}

		/* find the 0xff marker */
		const uint8_t *p = (const uint8_t *)memchr(data, 0xff, length);
		if (p == nullptr) {
			/* no marker - discard the buffer */
			decoder_buffer_consume(buffer, length);
			continue;
		}

		if (p > data) {
			/* discard data before 0xff */
			decoder_buffer_consume(buffer, p - data);
			continue;
		}

		/* is it a frame? */
		frame_length = adts_check_frame(data);
		if (frame_length == 0) {
			/* it's just some random 0xff byte; discard it
			   and continue searching */
			decoder_buffer_consume(buffer, 1);
			continue;
		}

		if (length < frame_length) {
			/* available buffer size is smaller than the
			   frame will be - attempt to read more
			   data */
			ret = decoder_buffer_fill(buffer);
			if (!ret) {
				/* not enough data; discard this frame
				   to prevent a possible buffer
				   overflow */
				data = (const uint8_t *)
					decoder_buffer_read(buffer, &length);
				if (data != nullptr)
					decoder_buffer_consume(buffer, length);
			}

			continue;
		}

		/* found a full frame! */
		return frame_length;
	}
}

static float
adts_song_duration(DecoderBuffer *buffer)
{
	unsigned int frames, frame_length;
	unsigned sample_rate = 0;
	float frames_per_second;

	/* Read all frames to ensure correct time and bitrate */
	for (frames = 0;; frames++) {
		frame_length = adts_find_frame(buffer);
		if (frame_length == 0)
			break;


		if (frames == 0) {
			size_t buffer_length;
			const uint8_t *data = (const uint8_t *)
				decoder_buffer_read(buffer, &buffer_length);
			assert(data != nullptr);
			assert(frame_length <= buffer_length);

			sample_rate = adts_sample_rates[(data[2] & 0x3c) >> 2];
		}

		decoder_buffer_consume(buffer, frame_length);
	}

	frames_per_second = (float)sample_rate / 1024.0;
	if (frames_per_second <= 0)
		return -1;

	return (float)frames / frames_per_second;
}

static float
faad_song_duration(DecoderBuffer *buffer, struct input_stream *is)
{
	size_t fileread;
	size_t tagsize;
	size_t length;
	bool success;

	const goffset size = input_stream_get_size(is);
	fileread = size >= 0 ? size : 0;

	decoder_buffer_fill(buffer);
	const uint8_t *data = (const uint8_t *)
		decoder_buffer_read(buffer, &length);
	if (data == nullptr)
		return -1;

	tagsize = 0;
	if (length >= 10 && !memcmp(data, "ID3", 3)) {
		/* skip the ID3 tag */

		tagsize = (data[6] << 21) | (data[7] << 14) |
		    (data[8] << 7) | (data[9] << 0);

		tagsize += 10;

		success = decoder_buffer_skip(buffer, tagsize) &&
			decoder_buffer_fill(buffer);
		if (!success)
			return -1;

		data = (const uint8_t *)decoder_buffer_read(buffer, &length);
		if (data == nullptr)
			return -1;
	}

	if (input_stream_is_seekable(is) && length >= 2 &&
	    data[0] == 0xFF && ((data[1] & 0xF6) == 0xF0)) {
		/* obtain the duration from the ADTS header */
		float song_length = adts_song_duration(buffer);

		input_stream_lock_seek(is, tagsize, SEEK_SET, nullptr);

		data = (const uint8_t *)decoder_buffer_read(buffer, &length);
		if (data != nullptr)
			decoder_buffer_consume(buffer, length);
		decoder_buffer_fill(buffer);

		return song_length;
	} else if (length >= 5 && memcmp(data, "ADIF", 4) == 0) {
		/* obtain the duration from the ADIF header */
		unsigned bit_rate;
		size_t skip_size = (data[4] & 0x80) ? 9 : 0;

		if (8 + skip_size > length)
			/* not enough data yet; skip parsing this
			   header */
			return -1;

		bit_rate = ((data[4 + skip_size] & 0x0F) << 19) |
			(data[5 + skip_size] << 11) |
			(data[6 + skip_size] << 3) |
			(data[7 + skip_size] & 0xE0);

		if (fileread != 0 && bit_rate != 0)
			return fileread * 8.0 / bit_rate;
		else
			return fileread;
	} else
		return -1;
}

/**
 * Wrapper for NeAACDecInit() which works around some API
 * inconsistencies in libfaad.
 */
static bool
faad_decoder_init(NeAACDecHandle decoder, DecoderBuffer *buffer,
		  struct audio_format *audio_format, GError **error_r)
{
	int32_t nbytes;
	uint32_t sample_rate;
	uint8_t channels;
#ifdef HAVE_FAAD_LONG
	/* neaacdec.h declares all arguments as "unsigned long", but
	   internally expects uint32_t pointers.  To avoid gcc
	   warnings, use this workaround. */
	unsigned long *sample_rate_p = (unsigned long *)(void *)&sample_rate;
#else
	uint32_t *sample_rate_p = &sample_rate;
#endif

	size_t length;
	const unsigned char *data = (const unsigned char *)
		decoder_buffer_read(buffer, &length);
	if (data == nullptr) {
		g_set_error(error_r, faad_decoder_quark(), 0,
			    "Empty file");
		return false;
	}

	nbytes = NeAACDecInit(decoder,
			      /* deconst hack, libfaad requires this */
			      const_cast<unsigned char *>(data),
			     length,
			     sample_rate_p, &channels);
	if (nbytes < 0) {
		g_set_error(error_r, faad_decoder_quark(), 0,
			    "Not an AAC stream");
		return false;
	}

	decoder_buffer_consume(buffer, nbytes);

	return audio_format_init_checked(audio_format, sample_rate,
					 SAMPLE_FORMAT_S16, channels, error_r);
}

/**
 * Wrapper for NeAACDecDecode() which works around some API
 * inconsistencies in libfaad.
 */
static const void *
faad_decoder_decode(NeAACDecHandle decoder, DecoderBuffer *buffer,
		    NeAACDecFrameInfo *frame_info)
{
	size_t length;
	const unsigned char *data = (const unsigned char *)
		decoder_buffer_read(buffer, &length);
	if (data == nullptr)
		return nullptr;

	return NeAACDecDecode(decoder, frame_info,
			      /* deconst hack, libfaad requires this */
			      const_cast<unsigned char *>(data),
			      length);
}

/**
 * Get a song file's total playing time in seconds, as a float.
 * Returns 0 if the duration is unknown, and a negative value if the
 * file is invalid.
 */
static float
faad_get_file_time_float(struct input_stream *is)
{
	DecoderBuffer *buffer;
	float length;

	buffer = decoder_buffer_new(nullptr, is,
				    FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
	length = faad_song_duration(buffer, is);

	if (length < 0) {
		bool ret;
		struct audio_format audio_format;

		NeAACDecHandle decoder = NeAACDecOpen();

		NeAACDecConfigurationPtr config =
			NeAACDecGetCurrentConfiguration(decoder);
		config->outputFormat = FAAD_FMT_16BIT;
		NeAACDecSetConfiguration(decoder, config);

		decoder_buffer_fill(buffer);

		ret = faad_decoder_init(decoder, buffer, &audio_format, nullptr);
		if (ret)
			length = 0;

		NeAACDecClose(decoder);
	}

	decoder_buffer_free(buffer);

	return length;
}

/**
 * Get a song file's total playing time in seconds, as an int.
 * Returns 0 if the duration is unknown, and a negative value if the
 * file is invalid.
 */
static int
faad_get_file_time(struct input_stream *is)
{
	int file_time = -1;
	float length;

	if ((length = faad_get_file_time_float(is)) >= 0)
		file_time = length + 0.5;

	return file_time;
}

static void
faad_stream_decode(struct decoder *mpd_decoder, struct input_stream *is)
{
	GError *error = nullptr;
	float total_time = 0;
	struct audio_format audio_format;
	bool ret;
	uint16_t bit_rate = 0;
	DecoderBuffer *buffer;
	enum decoder_command cmd;

	buffer = decoder_buffer_new(mpd_decoder, is,
				    FAAD_MIN_STREAMSIZE * AAC_MAX_CHANNELS);
	total_time = faad_song_duration(buffer, is);

	/* create the libfaad decoder */

	NeAACDecHandle decoder = NeAACDecOpen();

	NeAACDecConfigurationPtr config =
		NeAACDecGetCurrentConfiguration(decoder);
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 1;
	config->dontUpSampleImplicitSBR = 0;
	NeAACDecSetConfiguration(decoder, config);

	while (!decoder_buffer_is_full(buffer) &&
	       !input_stream_lock_eof(is) &&
	       decoder_get_command(mpd_decoder) == DECODE_COMMAND_NONE) {
		adts_find_frame(buffer);
		decoder_buffer_fill(buffer);
	}

	/* initialize it */

	ret = faad_decoder_init(decoder, buffer, &audio_format, &error);
	if (!ret) {
		g_warning("%s", error->message);
		g_error_free(error);
		NeAACDecClose(decoder);
		return;
	}

	/* initialize the MPD core */

	decoder_initialized(mpd_decoder, &audio_format, false, total_time);

	/* the decoder loop */

	do {
		size_t frame_size;
		const void *decoded;
		NeAACDecFrameInfo frame_info;

		/* find the next frame */

		frame_size = adts_find_frame(buffer);
		if (frame_size == 0)
			/* end of file */
			break;

		/* decode it */

		decoded = faad_decoder_decode(decoder, buffer, &frame_info);

		if (frame_info.error > 0) {
			g_warning("error decoding AAC stream: %s\n",
				  NeAACDecGetErrorMessage(frame_info.error));
			break;
		}

		if (frame_info.channels != audio_format.channels) {
			g_warning("channel count changed from %u to %u",
				  audio_format.channels, frame_info.channels);
			break;
		}

		if (frame_info.samplerate != audio_format.sample_rate) {
			g_warning("sample rate changed from %u to %lu",
				  audio_format.sample_rate,
				  (unsigned long)frame_info.samplerate);
			break;
		}

		decoder_buffer_consume(buffer, frame_info.bytesconsumed);

		/* update bit rate and position */

		if (frame_info.samples > 0) {
			bit_rate = frame_info.bytesconsumed * 8.0 *
			    frame_info.channels * audio_format.sample_rate /
			    frame_info.samples / 1000 + 0.5;
		}

		/* send PCM samples to MPD */

		cmd = decoder_data(mpd_decoder, is, decoded,
				   (size_t)frame_info.samples * 2,
				   bit_rate);
	} while (cmd != DECODE_COMMAND_STOP);

	/* cleanup */

	NeAACDecClose(decoder);
}

static bool
faad_scan_stream(struct input_stream *is,
		 const struct tag_handler *handler, void *handler_ctx)
{
	int file_time = faad_get_file_time(is);

	if (file_time < 0)
		return false;

	tag_handler_invoke_duration(handler, handler_ctx, file_time);
	return true;
}

static const char *const faad_suffixes[] = { "aac", nullptr };
static const char *const faad_mime_types[] = {
	"audio/aac", "audio/aacp", nullptr
};

const struct decoder_plugin faad_decoder_plugin = {
	"faad",
	nullptr,
	nullptr,
	faad_stream_decode,
	nullptr,
	nullptr,
	faad_scan_stream,
	nullptr,
	faad_suffixes,
	faad_mime_types,
};
