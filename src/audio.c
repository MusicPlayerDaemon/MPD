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

#include "audio.h"
#include "audio_format.h"
#include "output_api.h"
#include "output_control.h"
#include "output_internal.h"
#include "log.h"
#include "path.h"
#include "client.h"
#include "idle.h"
#include "utils.h"
#include "os_compat.h"

#include <glib.h>

#define AUDIO_DEVICE_STATE	"audio_device_state:"
#define AUDIO_BUFFER_SIZE	2*MPD_PATH_MAX

static struct audio_format configured_audio_format;
static struct audio_format input_audio_format;

static struct audio_output *audioOutputArray;
static unsigned int audioOutputArraySize;

static unsigned int audio_output_count(void)
{
	unsigned int nr = 0;
	ConfigParam *param = NULL;

	while ((param = getNextConfigParam(CONF_AUDIO_OUTPUT, param)))
		nr++;
	if (!nr)
		nr = 1; /* we'll always have at least one device  */
	return nr;
}

/* make sure initPlayerData is called before this function!! */
void initAudioDriver(void)
{
	ConfigParam *param = NULL;
	unsigned int i;

	audioOutputArraySize = audio_output_count();
	audioOutputArray = xmalloc(sizeof(struct audio_output) * audioOutputArraySize);

	for (i = 0; i < audioOutputArraySize; i++)
	{
		struct audio_output *output = &audioOutputArray[i];
		unsigned int j;

		param = getNextConfigParam(CONF_AUDIO_OUTPUT, param);

		/* only allow param to be NULL if there just one audioOutput */
		assert(param || (audioOutputArraySize == 1));

		if (!audio_output_init(output, param)) {
			if (param)
			{
				FATAL("problems configuring output device "
				      "defined at line %i\n", param->line);
			}
			else
			{
				FATAL("No audio_output specified and unable to "
				      "detect a default audio output device\n");
			}
		}

		/* require output names to be unique: */
		for (j = 0; j < i; j++) {
			if (!strcmp(output->name, audioOutputArray[j].name)) {
				FATAL("output devices with identical "
				      "names: %s\n", output->name);
			}
		}
	}
}

void getOutputAudioFormat(const struct audio_format *inAudioFormat,
			  struct audio_format *outAudioFormat)
{
	*outAudioFormat = audio_format_defined(&configured_audio_format)
		? configured_audio_format
		: *inAudioFormat;
}

void initAudioConfig(void)
{
	ConfigParam *param = getConfigParam(CONF_AUDIO_OUTPUT_FORMAT);

	if (NULL == param || NULL == param->value)
		return;

	if (0 != parseAudioConfig(&configured_audio_format, param->value)) {
		FATAL("error parsing \"%s\" at line %i\n",
		      CONF_AUDIO_OUTPUT_FORMAT, param->line);
	}
}

int parseAudioConfig(struct audio_format *audioFormat, char *conf)
{
	char *test;

	memset(audioFormat, 0, sizeof(*audioFormat));

	audioFormat->sample_rate = strtol(conf, &test, 10);

	if (*test != ':') {
		ERROR("error parsing audio output format: %s\n", conf);
		return -1;
	}

	if (audioFormat->sample_rate <= 0) {
		ERROR("sample rate %u is not >= 0\n",
		      audioFormat->sample_rate);
		return -1;
	}

	audioFormat->bits = (uint8_t)strtoul(test + 1, &test, 10);

	if (*test != ':') {
		ERROR("error parsing audio output format: %s\n", conf);
		return -1;
	}

	if (audioFormat->bits != 16 && audioFormat->bits != 24 &&
	    audioFormat->bits != 8) {
		ERROR("bits %u can not be used for audio output\n",
		      audioFormat->bits);
		return -1;
	}

	audioFormat->channels = (uint8_t)strtoul(test + 1, &test, 10);

	if (*test != '\0') {
		ERROR("error parsing audio output format: %s\n", conf);
		return -1;
	}

	switch (audioFormat->channels) {
	case 1:
	case 2:
		break;
	default:
		ERROR("channels %i can not be used for audio output\n",
		      (int)audioFormat->channels);
		return -1;
	}

	return 0;
}

void finishAudioConfig(void)
{
	audio_format_clear(&configured_audio_format);
}

void finishAudioDriver(void)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; i++) {
		audio_output_finish(&audioOutputArray[i]);
	}

	free(audioOutputArray);
	audioOutputArray = NULL;
	audioOutputArraySize = 0;
}

bool
isCurrentAudioFormat(const struct audio_format *audioFormat)
{
	assert(audioFormat != NULL);

	return audio_format_equals(audioFormat, &input_audio_format);
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

			if (audio_output_command_is_finished(ao)) {
				bool success = audio_output_get_result(ao);
				if (success)
					ret = true;
			} else {
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

int enableAudioDevice(unsigned int device)
{
	if (device >= audioOutputArraySize)
		return -1;

	audioOutputArray[device].enabled = true;
	idle_add(IDLE_OUTPUT);

	return 0;
}

int disableAudioDevice(unsigned int device)
{
	if (device >= audioOutputArraySize)
		return -1;

	audioOutputArray[device].enabled = false;
	idle_add(IDLE_OUTPUT);

	return 0;
}

void printAudioDevices(struct client *client)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; i++) {
		client_printf(client,
			      "outputid: %i\n"
			      "outputname: %s\n"
			      "outputenabled: %i\n",
			      i,
			      audioOutputArray[i].name,
			      audioOutputArray[i].enabled);
	}
}

void saveAudioDevicesState(FILE *fp)
{
	unsigned int i;

	assert(audioOutputArraySize != 0);
	for (i = 0; i < audioOutputArraySize; i++) {
		fprintf(fp, AUDIO_DEVICE_STATE "%d:%s\n",
			audioOutputArray[i].enabled,
		        audioOutputArray[i].name);
	}
}

void readAudioDevicesState(FILE *fp)
{
	char buffer[AUDIO_BUFFER_SIZE];
	unsigned int i;

	assert(audioOutputArraySize != 0);

	while (myFgets(buffer, AUDIO_BUFFER_SIZE, fp)) {
		char *c, *name;

		if (!g_str_has_prefix(buffer, AUDIO_DEVICE_STATE))
			continue;

		c = strchr(buffer, ':');
		if (!c || !(++c))
			goto errline;

		name = strchr(c, ':');
		if (!name || !(++name))
			goto errline;

		for (i = 0; i < audioOutputArraySize; ++i) {
			if (!strcmp(name, audioOutputArray[i].name)) {
				/* devices default to on */
				if (!atoi(c))
					audioOutputArray[i].enabled = false;
				break;
			}
		}
		continue;
errline:
		/* nonfatal */
		ERROR("invalid line in state_file: %s\n", buffer);
	}
}

