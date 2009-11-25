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
#include "audio_check.h"

#ifdef MPC_IS_OLD_API
#include <mpcdec/mpcdec.h>
#else
#include <mpc/mpcdec.h>
#endif

#include <glib.h>
#include <assert.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mpcdec"

struct mpc_decoder_data {
	struct input_stream *is;
	struct decoder *decoder;
};

#ifdef MPC_IS_OLD_API
#define cb_first_arg void *vdata
#define cb_data vdata
#else
#define cb_first_arg mpc_reader *reader
#define cb_data reader->data
#endif

static mpc_int32_t
mpc_read_cb(cb_first_arg, void *ptr, mpc_int32_t size)
{
	struct mpc_decoder_data *data = (struct mpc_decoder_data *) cb_data;

	return decoder_read(data->decoder, data->is, ptr, size);
}

static mpc_bool_t
mpc_seek_cb(cb_first_arg, mpc_int32_t offset)
{
	struct mpc_decoder_data *data = (struct mpc_decoder_data *) cb_data;

	return input_stream_seek(data->is, offset, SEEK_SET);
}

static mpc_int32_t
mpc_tell_cb(cb_first_arg)
{
	struct mpc_decoder_data *data = (struct mpc_decoder_data *) cb_data;

	return (long)(data->is->offset);
}

static mpc_bool_t
mpc_canseek_cb(cb_first_arg)
{
	struct mpc_decoder_data *data = (struct mpc_decoder_data *) cb_data;

	return data->is->seekable;
}

static mpc_int32_t
mpc_getsize_cb(cb_first_arg)
{
	struct mpc_decoder_data *data = (struct mpc_decoder_data *) cb_data;

	return data->is->size;
}

/* this _looks_ performance-critical, don't de-inline -- eric */
static inline int32_t
mpc_to_mpd_sample(MPC_SAMPLE_FORMAT sample)
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
		*dest++ = mpc_to_mpd_sample(*src++);
}

static void
mpcdec_decode(struct decoder *mpd_decoder, struct input_stream *is)
{
#ifdef MPC_IS_OLD_API
	mpc_decoder decoder;
#else
	mpc_demux *demux;
	mpc_frame_info frame;
	mpc_status status;
#endif
	mpc_reader reader;
	mpc_streaminfo info;
	GError *error = NULL;
	struct audio_format audio_format;

	struct mpc_decoder_data data;

	MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];

	mpc_uint32_t ret;
	int32_t chunk[G_N_ELEMENTS(sample_buffer)];
	long bit_rate = 0;
	unsigned long sample_pos = 0;
	mpc_uint32_t vbr_update_acc;
	mpc_uint32_t vbr_update_bits;
	float total_time;
	struct replay_gain_info *replay_gain_info = NULL;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	data.is = is;
	data.decoder = mpd_decoder;

	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

#ifdef MPC_IS_OLD_API
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
#else
	demux = mpc_demux_init(&reader);
	if (demux == NULL) {
		if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP)
			g_warning("Not a valid musepack stream");
		return;
	}

	mpc_demux_get_info(demux, &info);
#endif

	if (!audio_format_init_checked(&audio_format, info.sample_freq, 24,
				       info.channels, &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
#ifndef MPC_IS_OLD_API
		mpc_demux_exit(demux);
#endif
		return;
	}

	replay_gain_info = replay_gain_info_new();
	replay_gain_info->tuples[REPLAY_GAIN_ALBUM].gain = info.gain_album * 0.01;
	replay_gain_info->tuples[REPLAY_GAIN_ALBUM].peak = info.peak_album / 32767.0;
	replay_gain_info->tuples[REPLAY_GAIN_TRACK].gain = info.gain_title * 0.01;
	replay_gain_info->tuples[REPLAY_GAIN_TRACK].peak = info.peak_title / 32767.0;

	decoder_initialized(mpd_decoder, &audio_format,
			    is->seekable,
			    mpc_streaminfo_get_length(&info));

	do {
		if (cmd == DECODE_COMMAND_SEEK) {
			bool success;

			sample_pos = decoder_seek_where(mpd_decoder) *
				audio_format.sample_rate;

#ifdef MPC_IS_OLD_API
			success = mpc_decoder_seek_sample(&decoder,
							  sample_pos);
#else
			success = mpc_demux_seek_sample(demux, sample_pos)
				== MPC_STATUS_OK;
#endif
			if (success)
				decoder_command_finished(mpd_decoder);
			else
				decoder_seek_error(mpd_decoder);
		}

		vbr_update_acc = 0;
		vbr_update_bits = 0;

#ifdef MPC_IS_OLD_API
		ret = mpc_decoder_decode(&decoder, sample_buffer,
					 &vbr_update_acc, &vbr_update_bits);
		if (ret == 0 || ret == (mpc_uint32_t)-1)
			break;
#else
		frame.buffer = (MPC_SAMPLE_FORMAT *)sample_buffer;
		status = mpc_demux_decode(demux, &frame);
		if (status != MPC_STATUS_OK) {
			g_warning("Failed to decode sample");
			break;
		}

		if (frame.bits == -1)
			break;

		ret = frame.samples;
#endif

		sample_pos += ret;

		ret *= info.channels;

		mpc_to_mpd_buffer(chunk, sample_buffer, ret);

		total_time = ((float)sample_pos) / audio_format.sample_rate;
		bit_rate = vbr_update_bits * audio_format.sample_rate
			/ 1152 / 1000;

		cmd = decoder_data(mpd_decoder, is,
				   chunk, ret * sizeof(chunk[0]),
				   total_time,
				   bit_rate, replay_gain_info);
	} while (cmd != DECODE_COMMAND_STOP);

	replay_gain_info_free(replay_gain_info);

#ifndef MPC_IS_OLD_API
	mpc_demux_exit(demux);
#endif
}

static float
mpcdec_get_file_duration(const char *file)
{
	struct input_stream is;
	float total_time = -1;

	mpc_reader reader;
#ifndef MPC_IS_OLD_API
	mpc_demux *demux;
#endif
	mpc_streaminfo info;
	struct mpc_decoder_data data;

	if (!input_stream_open(&is, file))
		return -1;

	data.is = &is;
	data.decoder = NULL;

	reader.read = mpc_read_cb;
	reader.seek = mpc_seek_cb;
	reader.tell = mpc_tell_cb;
	reader.get_size = mpc_getsize_cb;
	reader.canseek = mpc_canseek_cb;
	reader.data = &data;

#ifdef MPC_IS_OLD_API
	mpc_streaminfo_init(&info);

	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		input_stream_close(&is);
		return -1;
	}
#else
	demux = mpc_demux_init(&reader);
	if (demux == NULL) {
		input_stream_close(&is);
		return -1;
	}

	mpc_demux_get_info(demux, &info);
	mpc_demux_exit(demux);
#endif

	total_time = mpc_streaminfo_get_length(&info);

	input_stream_close(&is);

	return total_time;
}

static struct tag *
mpcdec_tag_dup(const char *file)
{
	float total_time = mpcdec_get_file_duration(file);
	struct tag *tag;

	if (total_time < 0) {
		g_debug("Failed to get duration of file: %s", file);
		return NULL;
	}

	tag = tag_new();
	tag->time = total_time;
	return tag;
}

static const char *const mpcdec_suffixes[] = { "mpc", NULL };

const struct decoder_plugin mpcdec_decoder_plugin = {
	.name = "mpcdec",
	.stream_decode = mpcdec_decode,
	.tag_dup = mpcdec_tag_dup,
	.suffixes = mpcdec_suffixes,
};
