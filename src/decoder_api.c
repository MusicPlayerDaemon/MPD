/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#include "decoder_internal.h"
#include "audio.h"

#include "utils.h"
#include "normalize.h"
#include "playerData.h"
#include "gcc.h"

void decoder_initialized(struct decoder * decoder,
			 const AudioFormat * audio_format,
			 float total_time)
{
	assert(dc.state == DECODE_STATE_START);

	memset(&decoder->conv_state, 0, sizeof(decoder->conv_state));

	if (audio_format != NULL) {
		dc.audioFormat = *audio_format;
		getOutputAudioFormat(audio_format,
				     &(ob.audioFormat));
	}

	dc.totalTime = total_time;

	dc.state = DECODE_STATE_DECODE;
	notify_signal(&pc.notify);
}

const char *decoder_get_url(mpd_unused struct decoder * decoder, char * buffer)
{
	return get_song_url(buffer, dc.current_song);
}

enum decoder_command decoder_get_command(mpd_unused struct decoder * decoder)
{
	return dc.command;
}

void decoder_command_finished(mpd_unused struct decoder * decoder)
{
	assert(dc.command != DECODE_COMMAND_NONE);

	dc.command = DECODE_COMMAND_NONE;
	notify_signal(&pc.notify);
}

double decoder_seek_where(mpd_unused struct decoder * decoder)
{
	assert(dc.command == DECODE_COMMAND_SEEK);

	return dc.seekWhere;
}

void decoder_seek_error(struct decoder * decoder)
{
	assert(dc.command == DECODE_COMMAND_SEEK);

	dc.seekError = 1;
	decoder_command_finished(decoder);
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static enum decoder_command
need_chunks(struct decoder *decoder, InputStream * inStream, int seekable)
{
	if (dc.command == DECODE_COMMAND_STOP)
		return DECODE_COMMAND_STOP;

	if (dc.command == DECODE_COMMAND_SEEK) {
		if (seekable) {
			return DECODE_COMMAND_SEEK;
		} else {
			decoder_seek_error(decoder);
		}
	}

	if (!inStream ||
	    bufferInputStream(inStream) <= 0) {
		notify_wait(&dc.notify);
		notify_signal(&pc.notify);
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
decoder_data(struct decoder *decoder, InputStream * inStream, int seekable,
	     void *dataIn, size_t dataInLen,
	     float data_time, mpd_uint16 bitRate,
	     ReplayGainInfo * replayGainInfo)
{
	size_t nbytes;
	char *data;
	size_t datalen;
	static char *convBuffer;
	static size_t convBufferLen;
	int ret;

	if (cmpAudioFormat(&(ob.audioFormat), &(dc.audioFormat)) == 0) {
		data = dataIn;
		datalen = dataInLen;
	} else {
		datalen = pcm_sizeOfConvBuffer(&(dc.audioFormat), dataInLen,
		                               &(ob.audioFormat));
		if (datalen > convBufferLen) {
			if (convBuffer != NULL)
				free(convBuffer);
			convBuffer = xmalloc(datalen);
			convBufferLen = datalen;
		}
		data = convBuffer;
		datalen = pcm_convertAudioFormat(&(dc.audioFormat), dataIn,
		                                 dataInLen, &(ob.audioFormat),
		                                 data, &decoder->conv_state);
	}

	if (replayGainInfo != NULL && (replayGainState != REPLAYGAIN_OFF))
		doReplayGain(replayGainInfo, data, datalen, &ob.audioFormat);
	else if (normalizationEnabled)
		normalizeData(data, datalen, &ob.audioFormat);

	while (datalen > 0) {
		nbytes = ob_append(data, datalen, data_time, bitRate);
		datalen -= nbytes;
		data += nbytes;

		if (datalen > 0) {
			ret = need_chunks(decoder, inStream, seekable);
			if (ret != 0)
				return ret;
		}
	}

	return DECODE_COMMAND_NONE;
}

void decoder_flush(mpd_unused struct decoder *decoder)
{
	ob_flush();
}

void decoder_clear(mpd_unused struct decoder *decoder)
{
	ob_clear();
}
