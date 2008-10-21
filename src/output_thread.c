/* the Music Player Daemon (MPD)
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

#include "output_thread.h"
#include "output_api.h"
#include "output_internal.h"
#include "utils.h"

#include <assert.h>

static void ao_command_finished(struct audio_output *ao)
{
	assert(ao->command != AO_COMMAND_NONE);
	ao->command = AO_COMMAND_NONE;
	notify_signal(&audio_output_client_notify);
}

static void convertAudioFormat(struct audio_output *audioOutput,
			       const char **chunkArgPtr, size_t *sizeArgPtr)
{
	size_t size = pcm_convert_size(&(audioOutput->inAudioFormat),
				       *sizeArgPtr,
				       &(audioOutput->outAudioFormat));

	if (size > audioOutput->convBufferLen) {
		if (audioOutput->convBuffer != NULL)
			free(audioOutput->convBuffer);
		audioOutput->convBuffer = xmalloc(size);
		audioOutput->convBufferLen = size;
	}

	*sizeArgPtr = pcm_convert(&(audioOutput->inAudioFormat),
				  *chunkArgPtr, *sizeArgPtr,
				  &(audioOutput->outAudioFormat),
				  audioOutput->convBuffer,
				  &audioOutput->convState);

	*chunkArgPtr = audioOutput->convBuffer;
}

static void ao_play(struct audio_output *ao)
{
	const char *data = ao->args.play.data;
	size_t size = ao->args.play.size;

	if (!audio_format_equals(&ao->inAudioFormat, &ao->outAudioFormat))
		convertAudioFormat(ao, &data, &size);

	ao->result = ao->plugin->play(ao->data, data, size);
	ao_command_finished(ao);
}

static void ao_pause(struct audio_output *ao)
{
	if (ao->plugin->pause != NULL) {
		/* pause is supported */
		ao_command_finished(ao);
		ao->plugin->pause(ao->data);
	} else {
		/* pause is not supported - simply close the device */
		ao->plugin->close(ao->data);
		ao->open = 0;
		ao_command_finished(ao);
	}
}

static void *audio_output_task(void *arg)
{
	struct audio_output *ao = arg;

	while (1) {
		switch (ao->command) {
		case AO_COMMAND_NONE:
			break;

		case AO_COMMAND_OPEN:
			assert(!ao->open);
			ao->result = ao->plugin->open(ao->data,
						      &ao->outAudioFormat);

			assert(!ao->open);
			if (ao->result == 0)
				ao->open = 1;

			ao_command_finished(ao);
			break;

		case AO_COMMAND_CLOSE:
			assert(ao->open);
			ao->plugin->close(ao->data);
			ao->open = 0;
			ao_command_finished(ao);
			break;

		case AO_COMMAND_PLAY:
			ao_play(ao);
			break;

		case AO_COMMAND_PAUSE:
			ao_pause(ao);
			break;

		case AO_COMMAND_CANCEL:
			ao->plugin->cancel(ao->data);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_SEND_TAG:
			ao->plugin->send_tag(ao->data, ao->args.tag);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_KILL:
			ao_command_finished(ao);
			return NULL;
		}

		notify_wait(&ao->notify);
	}
}

void audio_output_thread_start(struct audio_output *ao)
{
	pthread_attr_t attr;

	assert(ao->command == AO_COMMAND_NONE);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&ao->thread, &attr, audio_output_task, ao))
		FATAL("Failed to spawn output task: %s\n", strerror(errno));
}
