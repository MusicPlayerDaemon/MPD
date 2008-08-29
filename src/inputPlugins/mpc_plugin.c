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

#ifdef HAVE_MPCDEC

#include "../utils.h"
#include "../log.h"

#include <mpcdec/mpcdec.h>

typedef struct _MpcCallbackData {
	InputStream *inStream;
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

	return seekInputStream(data->inStream, offset, SEEK_SET) < 0 ? 0 : 1;
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
static inline mpd_sint16 convertSample(MPC_SAMPLE_FORMAT sample)
{
	/* only doing 16-bit audio for now */
	mpd_sint32 val;

	const int clip_min = -1 << (16 - 1);
	const int clip_max = (1 << (16 - 1)) - 1;

#ifdef MPC_FIXED_POINT
	const int shift = 16 - MPC_FIXED_POINT_SCALE_SHIFT;

	if (sample > 0) {
		sample <<= shift;
	} else if (shift < 0) {
		sample >>= -shift;
	}
	val = sample;
#else
	const int float_scale = 1 << (16 - 1);

	val = sample * float_scale;
#endif

	if (val < clip_min)
		val = clip_min;
	else if (val > clip_max)
		val = clip_max;

	return val;
}

static int mpc_decode(struct decoder * mpd_decoder, InputStream * inStream)
{
	mpc_decoder decoder;
	mpc_reader reader;
	mpc_streaminfo info;
	AudioFormat audio_format;

	MpcCallbackData data;

	MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];

	int eof = 0;
	long ret;
#define MPC_CHUNK_SIZE 4096
	char chunk[MPC_CHUNK_SIZE];
	int chunkpos = 0;
	long bitRate = 0;
	mpd_sint16 *s16 = (mpd_sint16 *) chunk;
	unsigned long samplePos = 0;
	mpc_uint32_t vbrUpdateAcc;
	mpc_uint32_t vbrUpdateBits;
	float total_time;
	int i;
	ReplayGainInfo *replayGainInfo = NULL;

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
		if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP) {
			ERROR("Not a valid musepack stream\n");
			return -1;
		}
		return 0;
	}

	mpc_decoder_setup(&decoder, &reader);

	if (!mpc_decoder_initialize(&decoder, &info)) {
		if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP) {
			ERROR("Not a valid musepack stream\n");
			return -1;
		}
		return 0;
	}

	audio_format.bits = 16;
	audio_format.channels = info.channels;
	audio_format.sampleRate = info.sample_freq;

	replayGainInfo = newReplayGainInfo();
	replayGainInfo->albumGain = info.gain_album * 0.01;
	replayGainInfo->albumPeak = info.peak_album / 32767.0;
	replayGainInfo->trackGain = info.gain_title * 0.01;
	replayGainInfo->trackPeak = info.peak_title / 32767.0;

	decoder_initialized(mpd_decoder, &audio_format,
			    mpc_streaminfo_get_length(&info));

	while (!eof) {
		if (decoder_get_command(mpd_decoder) == DECODE_COMMAND_SEEK) {
			samplePos = decoder_seek_where(mpd_decoder) *
				audio_format.sampleRate;
			if (mpc_decoder_seek_sample(&decoder, samplePos)) {
				decoder_clear(mpd_decoder);
				s16 = (mpd_sint16 *) chunk;
				chunkpos = 0;
				decoder_command_finished(mpd_decoder);
			} else
				decoder_seek_error(mpd_decoder);
		}

		vbrUpdateAcc = 0;
		vbrUpdateBits = 0;
		ret = mpc_decoder_decode(&decoder, sample_buffer,
					 &vbrUpdateAcc, &vbrUpdateBits);

		if (ret <= 0 || decoder_get_command(mpd_decoder) == DECODE_COMMAND_STOP) {
			eof = 1;
			break;
		}

		samplePos += ret;

		/* ret is in samples, and we have stereo */
		ret *= 2;

		for (i = 0; i < ret; i++) {
			/* 16 bit audio again */
			*s16 = convertSample(sample_buffer[i]);
			chunkpos += 2;
			s16++;

			if (chunkpos >= MPC_CHUNK_SIZE) {
				total_time = ((float)samplePos) /
				    audio_format.sampleRate;

				bitRate = vbrUpdateBits *
				    audio_format.sampleRate / 1152 / 1000;

				decoder_data(mpd_decoder, inStream,
					     inStream->seekable,
					     chunk, chunkpos,
					     total_time,
					     bitRate, replayGainInfo);

				chunkpos = 0;
				s16 = (mpd_sint16 *) chunk;
				if (decoder_get_command(mpd_decoder) == DECODE_COMMAND_STOP) {
					eof = 1;
					break;
				}
			}
		}
	}

	if (decoder_get_command(mpd_decoder) != DECODE_COMMAND_STOP &&
	    chunkpos > 0) {
		total_time = ((float)samplePos) / audio_format.sampleRate;

		bitRate =
		    vbrUpdateBits * audio_format.sampleRate / 1152 / 1000;

		decoder_data(mpd_decoder, NULL, inStream->seekable,
			     chunk, chunkpos, total_time, bitRate,
			     replayGainInfo);
	}

	decoder_flush(mpd_decoder);

	freeReplayGainInfo(replayGainInfo);

	return 0;
}

static float mpcGetTime(char *file)
{
	InputStream inStream;
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

	if (openInputStream(&inStream, file) < 0) {
		DEBUG("mpcGetTime: Failed to open file: %s\n", file);
		return -1;
	}

	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
		closeInputStream(&inStream);
		return -1;
	}

	total_time = mpc_streaminfo_get_length(&info);

	closeInputStream(&inStream);

	return total_time;
}

static struct tag *mpcTagDup(char *file)
{
	struct tag *ret = NULL;
	float total_time = mpcGetTime(file);

	if (total_time < 0) {
		DEBUG("mpcTagDup: Failed to get Songlength of file: %s\n",
		      file);
		return NULL;
	}

	ret = apeDup(file);
	if (!ret)
		ret = id3Dup(file);
	if (!ret)
		ret = newMpdTag();
	ret->time = total_time;

	return ret;
}

static const char *mpcSuffixes[] = { "mpc", NULL };

struct decoder_plugin mpcPlugin = {
	"mpc",
	NULL,
	NULL,
	NULL,
	mpc_decode,
	NULL,
	mpcTagDup,
	INPUT_PLUGIN_STREAM_URL | INPUT_PLUGIN_STREAM_FILE,
	mpcSuffixes,
	NULL
};

#else

struct decoder_plugin mpcPlugin;

#endif /* HAVE_MPCDEC */
