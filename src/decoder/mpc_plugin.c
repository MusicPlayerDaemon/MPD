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

#include <mpcdec/mpcdec.h>
#include <glib.h>
#include <unistd.h>

typedef struct _MpcCallbackData {
	struct input_stream *inStream;
	struct decoder *decoder;
} MpcCallbackData;

static mpc_int32_t mpc_read_cb(void *vdata, void *ptr, mpc_int32_t size)
{
	MpcCallbackData *data = (MpcCallbackData *) vdata;

	return decoder_read(data->decoder, data->inStream, ptr, size);
}

static mpc_bool_t mpc_seek_cb(void *vdata, mpc_int32_t offset)
{
	MpcCallbackData *data = (MpcCallbackData *) vdata;

	return input_stream_seek(data->inStream, offset, SEEK_SET);
}

static mpc_int32_t mpc_tell_cb(void *vdata)
{
	MpcCallbackData *data = (MpcCallbackData *) vdata;

	return (long)(data->inStream->offset);
}

static mpc_bool_t mpc_canseek_cb(void *vdata)
{
	MpcCallbackData *data = (MpcCallbackData *) vdata;

	return data->inStream->seekable;
}

static mpc_int32_t mpc_getsize_cb(void *vdata)
{
	MpcCallbackData *data = (MpcCallbackData *) vdata;

	return data->inStream->size;
}

/* this _looks_ performance-critical, don't de-inline -- eric */
static inline int32_t convertSample(MPC_SAMPLE_FORMAT sample)
{
	/* only doing 16-bit audio for now */
	int32_t val;

	enum {
		bits = 24,
	};

	const int clip_min = -1 << (bits - 1);
	const int clip_max = (1 << (bits - 1)) - 1;

#ifdef MPC_FIXED_POINT
	const int shift = bits - MPC_FIXED_POINT_SCALE_SHIFT;

	if (shift < 0)
		val = sample << -shift;
	else
		val = sample << shift;
#else
	const int float_scale = 1 << (bits - 1);

	val = sample * float_scale;
#endif

	if (val < clip_min)
		val = clip_min;
	else if (val > clip_max)
		val = clip_max;

	return val;
}

static void
mpc_to_mpd_buffer(int32_t *dest, const MPC_SAMPLE_FORMAT *src,
		  unsigned num_samples)
{
	while (num_samples-- > 0)
		*dest++ = convertSample(*src++);
}

static void
mpc_decode(struct decoder *mpd_decoder, struct input_stream *inStream)
{
	mpc_decoder decoder;
	mpc_reader reader;
	mpc_streaminfo info;
	struct audio_format audio_format;

	MpcCallbackData data;

	MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];

	mpc_uint32_t ret;
	int32_t chunk[G_N_ELEMENTS(sample_buffer)];
	long bitRate = 0;
	unsigned long samplePos = 0;
	mpc_uint32_t vbrUpdateAcc;
	mpc_uint32_t vbrUpdateBits;
	float total_time;
	struct replay_gain_info *replayGainInfo = NULL;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	data.inStream = inStream;
	data.decoder = mpd_decoder;

	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_streaminfo_init(&info);

	if ((ret = mpc_streaminfo_read(&info, &reader)) != ERROR_CODE_OK) {
		if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP)
			g_warning("Not a valid musepack stream\n");
		return;
	}

	mpc_decoder_setup(&decoder, &reader);

	if (!mpc_decoder_initialize(&decoder, &info)) {
		if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP)
			g_warning("Not a valid musepack stream\n");
		return;
	}

	audio_format.bits = 24;
	audio_format.channels = info.channels;
	audio_format.sample_rate = info.sample_freq;

	if (!audio_format_valid(&audio_format)) {
		g_warning("Invalid audio format: %u:%u:%u\n",
			  audio_format.sample_rate,
			  audio_format.bits,
			  audio_format.channels);
		return;
	}

	replayGainInfo = replay_gain_info_new();
	replayGainInfo->tuples[REPLAY_GAIN_ALBUM].gain = info.gain_album * 0.01;
	replayGainInfo->tuples[REPLAY_GAIN_ALBUM].peak = info.peak_album / 32767.0;
	replayGainInfo->tuples[REPLAY_GAIN_TRACK].gain = info.gain_title * 0.01;
	replayGainInfo->tuples[REPLAY_GAIN_TRACK].peak = info.peak_title / 32767.0;

	decoder_initialized(mpd_decoder, &audio_format,
			    inStream->seekable,
			    mpc_streaminfo_get_length(&info));

	do {
		if (cmd == DECODE_COMMAND_SEEK) {
			samplePos = decoder_seek_where(mpd_decoder) *
				audio_format.sample_rate;
			if (mpc_decoder_seek_sample(&decoder, samplePos))
				decoder_command_finished(mpd_decoder);
			else
				decoder_seek_error(mpd_decoder);
		}

		vbrUpdateAcc = 0;
		vbrUpdateBits = 0;
		ret = mpc_decoder_decode(&decoder, sample_buffer,
					 &vbrUpdateAcc, &vbrUpdateBits);
		if (ret == 0 || ret == (mpc_uint32_t)-1)
			break;

		samplePos += ret;

		ret *= info.channels;

		mpc_to_mpd_buffer(chunk, sample_buffer, ret);

		total_time = ((float)samplePos) / audio_format.sample_rate;
		bitRate = vbrUpdateBits * audio_format.sample_rate
			/ 1152 / 1000;

		cmd = decoder_data(mpd_decoder, inStream,
				   chunk, ret * sizeof(chunk[0]),
				   total_time,
				   bitRate, replayGainInfo);
	} while (cmd != DECODE_COMMAND_STOP);

	replay_gain_info_free(replayGainInfo);
}

static float mpcGetTime(const char *file)
{
	struct input_stream inStream;
	float total_time = -1;

	mpc_reader reader;
	mpc_streaminfo info;
	MpcCallbackData data;

	data.inStream = &inStream;
	data.decoder = NULL;

	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

	mpc_streaminfo_init(&info);

	if (!input_stream_open(&inStream, file))
		return -1;

	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		input_stream_close(&inStream);
		return -1;
	}

	total_time = mpc_streaminfo_get_length(&info);

	input_stream_close(&inStream);

	return total_time;
}

static struct tag *mpcTagDup(const char *file)
{
	struct tag *ret = NULL;
	float total_time = mpcGetTime(file);

	if (total_time < 0) {
		g_debug("mpcTagDup: Failed to get Songlength of file: %s\n",
			file);
		return NULL;
	}

	ret = tag_ape_load(file);
	if (!ret)
		ret = tag_id3_load(file);
	if (!ret)
		ret = tag_new();
	ret->time = total_time;

	return ret;
}

static const char *const mpcSuffixes[] = { "mpc", NULL };

const struct decoder_plugin mpcPlugin = {
	.name = "mpc",
	.stream_decode = mpc_decode,
	.tag_dup = mpcTagDup,
	.suffixes = mpcSuffixes,
};
