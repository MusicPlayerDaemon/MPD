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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "output_all.h"
#include "output_internal.h"
#include "output_control.h"

#include <assert.h>

static struct audio_format input_audio_format;

static struct audio_output *audioOutputArray;
static unsigned int audioOutputArraySize;

unsigned int audio_output_count(void)
{
	return audioOutputArraySize;
}

struct audio_output *
audio_output_get(unsigned i)
{
	assert(i < audioOutputArraySize);

	return &audioOutputArray[i];
}

struct audio_output *
audio_output_find(const char *name)
{
	for (unsigned i = 0; i < audioOutputArraySize; ++i) {
		struct audio_output *ao = audio_output_get(i);

		if (strcmp(ao->name, name) == 0)
			return ao;
	}

	/* name not found */
	return NULL;
}

static unsigned
audio_output_config_count(void)
{
	unsigned int nr = 0;
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_AUDIO_OUTPUT, param)))
		nr++;
	if (!nr)
		nr = 1; /* we'll always have at least one device  */
	return nr;
}

/* make sure initPlayerData is called before this function!! */
void initAudioDriver(void)
{
	const struct config_param *param = NULL;
	unsigned int i;

	notify_init(&audio_output_client_notify);

	audioOutputArraySize = audio_output_config_count();
	audioOutputArray = g_new(struct audio_output, audioOutputArraySize);

	for (i = 0; i < audioOutputArraySize; i++)
	{
		struct audio_output *output = &audioOutputArray[i];
		unsigned int j;

		param = config_get_next_param(CONF_AUDIO_OUTPUT, param);

		/* only allow param to be NULL if there just one audioOutput */
		assert(param || (audioOutputArraySize == 1));

		if (!audio_output_init(output, param)) {
			if (param)
			{
				g_error("problems configuring output device "
					"defined at line %i\n", param->line);
			}
			else
			{
				g_error("No audio_output specified and unable to "
					"detect a default audio output device\n");
			}
		}

		/* require output names to be unique: */
		for (j = 0; j < i; j++) {
			if (!strcmp(output->name, audioOutputArray[j].name)) {
				g_error("output devices with identical "
					"names: %s\n", output->name);
			}
		}
	}
}

void finishAudioDriver(void)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; i++) {
		audio_output_finish(&audioOutputArray[i]);
	}

	g_free(audioOutputArray);
	audioOutputArray = NULL;
	audioOutputArraySize = 0;

	notify_deinit(&audio_output_client_notify);
}

static void audio_output_wait_all(void)
{
	unsigned i;

	while (1) {
		int finished = 1;

		for (i = 0; i < audioOutputArraySize; ++i)
			if (audio_output_is_open(&audioOutputArray[i]) &&
			    !audio_output_command_is_finished(&audioOutputArray[i]))
				finished = 0;

		if (finished)
			break;

		notify_wait(&audio_output_client_notify);
	};
}

static void syncAudioDeviceStates(void)
{
	unsigned int i;

	if (!audio_format_defined(&input_audio_format))
		return;

	for (i = 0; i < audioOutputArraySize; ++i)
		audio_output_update(&audioOutputArray[i], &input_audio_format);
}

bool playAudio(const char *buffer, size_t length)
{
	bool ret = false;
	unsigned int i;

	assert(length > 0);
	/* no partial frames allowed */
	assert((length % audio_format_frame_size(&input_audio_format)) == 0);

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i)
		if (audio_output_is_open(&audioOutputArray[i]))
			audio_output_play(&audioOutputArray[i],
					  buffer, length);

	while (true) {
		bool finished = true;

		for (i = 0; i < audioOutputArraySize; ++i) {
			struct audio_output *ao = &audioOutputArray[i];

			if (!audio_output_is_open(ao))
				continue;

			if (audio_output_command_is_finished(ao))
				ret = true;
			else {
				finished = false;
				audio_output_signal(ao);
			}
		}

		if (finished)
			break;

		notify_wait(&audio_output_client_notify);
	};

	return ret;
}

bool openAudioDevice(const struct audio_format *audioFormat)
{
	bool ret = false;
	unsigned int i;

	if (!audioOutputArray)
		return false;

	if (audioFormat != NULL)
		input_audio_format = *audioFormat;

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audioOutputArray[i].open)
			ret = true;
	}

	if (!ret)
		/* close all devices if there was an error */
		closeAudioDevice();

	return ret;
}

void audio_output_pause_all(void)
{
	unsigned int i;

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i)
		if (audio_output_is_open(&audioOutputArray[i]))
			audio_output_pause(&audioOutputArray[i]);

	audio_output_wait_all();
}

void dropBufferedAudio(void)
{
	unsigned int i;

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audio_output_is_open(&audioOutputArray[i]))
			audio_output_cancel(&audioOutputArray[i]);
	}

	audio_output_wait_all();
}

void closeAudioDevice(void)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; ++i)
		audio_output_close(&audioOutputArray[i]);
}

void sendMetadataToAudioDevice(const struct tag *tag)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; ++i)
		if (audio_output_is_open(&audioOutputArray[i]))
			audio_output_send_tag(&audioOutputArray[i], tag);

	audio_output_wait_all();
}
