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

#include "output_control.h"
#include "output_api.h"
#include "pcm_utils.h"
#include "utils.h"

int audio_output_open(struct audio_output *audioOutput,
		      const struct audio_format *audioFormat)
{
	int ret = 0;

	if (audioOutput->open &&
	    audio_format_equals(audioFormat, &audioOutput->inAudioFormat)) {
		return 0;
	}

	audioOutput->inAudioFormat = *audioFormat;

	if (audioOutput->convertAudioFormat) {
		audioOutput->outAudioFormat = audioOutput->reqAudioFormat;
	} else {
		audioOutput->outAudioFormat = audioOutput->inAudioFormat;
		if (audioOutput->open)
			audio_output_close(audioOutput);
	}

	if (!audioOutput->open)
		ret = audioOutput->plugin->open(audioOutput);

	audioOutput->sameInAndOutFormats =
		audio_format_equals(&audioOutput->inAudioFormat,
		                    &audioOutput->outAudioFormat);

	return ret;
}

static void convertAudioFormat(struct audio_output *audioOutput,
			       const char **chunkArgPtr, size_t *sizeArgPtr)
{
	size_t size = pcm_sizeOfConvBuffer(&(audioOutput->inAudioFormat),
					   *sizeArgPtr,
					   &(audioOutput->outAudioFormat));

	if (size > audioOutput->convBufferLen) {
		if (audioOutput->convBuffer != NULL)
			free(audioOutput->convBuffer);
		audioOutput->convBuffer = xmalloc(size);
		audioOutput->convBufferLen = size;
	}

	*sizeArgPtr = pcm_convertAudioFormat(&(audioOutput->inAudioFormat), 
	                                     *chunkArgPtr, *sizeArgPtr, 
	                                     &(audioOutput->outAudioFormat),
	                                     audioOutput->convBuffer,
	                                     &audioOutput->convState);

	*chunkArgPtr = audioOutput->convBuffer;
}

int audio_output_play(struct audio_output *audioOutput,
		      const char *playChunk, size_t size)
{
	int ret;

	if (!audioOutput->open)
		return -1;

	if (!audioOutput->sameInAndOutFormats) {
		convertAudioFormat(audioOutput, &playChunk, &size);
	}

	ret = audioOutput->plugin->play(audioOutput, playChunk, size);

	return ret;
}

void audio_output_cancel(struct audio_output *audioOutput)
{
	if (audioOutput->open)
		audioOutput->plugin->cancel(audioOutput);
}

void audio_output_close(struct audio_output *audioOutput)
{
	if (audioOutput->open)
		audioOutput->plugin->close(audioOutput);
}

void audio_output_finish(struct audio_output *audioOutput)
{
	audio_output_close(audioOutput);
	if (audioOutput->plugin->finish)
		audioOutput->plugin->finish(audioOutput);
	if (audioOutput->convBuffer)
		free(audioOutput->convBuffer);
}

void audio_output_send_tag(struct audio_output *audioOutput,
			   const struct tag *tag)
{
	if (audioOutput->plugin->send_tag)
		audioOutput->plugin->send_tag(audioOutput, tag);
}
