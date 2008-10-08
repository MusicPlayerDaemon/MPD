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
#include "output_internal.h"
#include "output_thread.h"
#include "pcm_utils.h"

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

struct notify audio_output_client_notify = NOTIFY_INITIALIZER;

static void ao_command_wait(struct audio_output *ao)
{
	while (ao->command != AO_COMMAND_NONE) {
		notify_signal(&ao->notify);
		notify_wait(&audio_output_client_notify);
	}
}

static void ao_command(struct audio_output *ao, enum audio_output_command cmd)
{
	assert(ao->command == AO_COMMAND_NONE);
	ao->command = cmd;
	ao_command_wait(ao);
}

static void ao_command_async(struct audio_output *ao,
			     enum audio_output_command cmd)
{
	assert(ao->command == AO_COMMAND_NONE);
	ao->command = cmd;
	notify_signal(&ao->notify);
}

int audio_output_open(struct audio_output *audioOutput,
		      const struct audio_format *audioFormat)
{
	int ret = 0;

	if (audioOutput->open &&
	    audio_format_equals(audioFormat, &audioOutput->inAudioFormat)) {
		return 0;
	}

	audioOutput->inAudioFormat = *audioFormat;

	if (audio_format_defined(&audioOutput->reqAudioFormat)) {
		/* copy reqAudioFormat to outAudioFormat only if the
		   device is not yet open; if it is already open,
		   plugin->open() may have modified outAudioFormat,
		   and the value is already ok */
		if (!audioOutput->open)
			audioOutput->outAudioFormat =
				audioOutput->reqAudioFormat;
	} else {
		audioOutput->outAudioFormat = audioOutput->inAudioFormat;
		if (audioOutput->open)
			audio_output_close(audioOutput);
	}

	if (audioOutput->thread == 0)
		audio_output_thread_start(audioOutput);

	if (!audioOutput->open) {
		ao_command(audioOutput, AO_COMMAND_OPEN);
		ret = audioOutput->result;
	}

	return ret;
}

void
audio_output_signal(struct audio_output *ao)
{
	notify_signal(&ao->notify);
}

void audio_output_play(struct audio_output *audioOutput,
		       const char *playChunk, size_t size)
{
	if (!audioOutput->open)
		return;

	audioOutput->args.play.data = playChunk;
	audioOutput->args.play.size = size;
	ao_command_async(audioOutput, AO_COMMAND_PLAY);
}

void audio_output_pause(struct audio_output *audioOutput)
{
	ao_command_async(audioOutput, AO_COMMAND_PAUSE);
}

void audio_output_cancel(struct audio_output *audioOutput)
{
	ao_command_async(audioOutput, AO_COMMAND_CANCEL);
}

void audio_output_close(struct audio_output *audioOutput)
{
	if (audioOutput->open)
		ao_command(audioOutput, AO_COMMAND_CLOSE);
}

void audio_output_finish(struct audio_output *audioOutput)
{
	audio_output_close(audioOutput);
	if (audioOutput->thread != 0)
		ao_command(audioOutput, AO_COMMAND_KILL);
	if (audioOutput->plugin->finish)
		audioOutput->plugin->finish(audioOutput->data);
	if (audioOutput->convBuffer)
		free(audioOutput->convBuffer);
}

void audio_output_send_tag(struct audio_output *audioOutput,
			   const struct tag *tag)
{
	if (audioOutput->plugin->send_tag == NULL)
		return;

	audioOutput->args.tag = tag;
	ao_command_async(audioOutput, AO_COMMAND_SEND_TAG);
}
