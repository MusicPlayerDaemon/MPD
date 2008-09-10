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
#include "log.h"
#include "path.h"
#include "client.h"
#include "utils.h"
#include "os_compat.h"

#define AUDIO_DEVICE_STATE	"audio_device_state:"
#define AUDIO_DEVICE_STATE_LEN	(sizeof(AUDIO_DEVICE_STATE)-1)
#define AUDIO_BUFFER_SIZE	2*MPD_PATH_MAX

static struct audio_format audio_format;

static struct audio_format *audio_configFormat;

static struct audio_output *audioOutputArray;
static unsigned int audioOutputArraySize;

enum ad_state {
	DEVICE_OFF = 0x00,
	DEVICE_ENABLE = 0x01,  /* currently off, but to be turned on */
	DEVICE_ON = 0x03,
	DEVICE_DISABLE = 0x04   /* currently on, but to be turned off */
};

/* the audioEnabledArray should be stuck into shared memory, and then disable
   and enable in playAudio() routine */
static enum ad_state *audioDeviceStates;

static mpd_uint8 audioOpened;

static size_t audioBufferSize;
static char *audioBuffer;
static size_t audioBufferPos;

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
	audioDeviceStates = xmalloc(sizeof(enum ad_state) *
	                            audioOutputArraySize);
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
		audioDeviceStates[i] = DEVICE_ENABLE;
	}
}

void getOutputAudioFormat(const struct audio_format *inAudioFormat,
			  struct audio_format *outAudioFormat)
{
	*outAudioFormat = audio_configFormat != NULL
		? *audio_configFormat
		: *inAudioFormat;
}

void initAudioConfig(void)
{
	ConfigParam *param = getConfigParam(CONF_AUDIO_OUTPUT_FORMAT);

	if (NULL == param || NULL == param->value)
		return;

	audio_configFormat = xmalloc(sizeof(*audio_configFormat));

	if (0 != parseAudioConfig(audio_configFormat, param->value)) {
		FATAL("error parsing \"%s\" at line %i\n",
		      CONF_AUDIO_OUTPUT_FORMAT, param->line);
	}
}

int parseAudioConfig(struct audio_format *audioFormat, char *conf)
{
	char *test;

	memset(audioFormat, 0, sizeof(*audioFormat));

	audioFormat->sampleRate = strtol(conf, &test, 10);

	if (*test != ':') {
		ERROR("error parsing audio output format: %s\n", conf);
		return -1;
	}

	/*switch(audioFormat->sampleRate) {
	   case 48000:
	   case 44100:
	   case 32000:
	   case 16000:
	   break;
	   default:
	   ERROR("sample rate %i can not be used for audio output\n",
	   (int)audioFormat->sampleRate);
	   return -1
	   } */

	if (audioFormat->sampleRate <= 0) {
		ERROR("sample rate %i is not >= 0\n",
		      (int)audioFormat->sampleRate);
		return -1;
	}

	audioFormat->bits = (mpd_sint8)strtol(test + 1, &test, 10);

	if (*test != ':') {
		ERROR("error parsing audio output format: %s\n", conf);
		return -1;
	}

	switch (audioFormat->bits) {
	case 16:
		break;
	default:
		ERROR("bits %i can not be used for audio output\n",
		      (int)audioFormat->bits);
		return -1;
	}

	audioFormat->channels = (mpd_sint8)strtol(test + 1, &test, 10);

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
	if (audio_configFormat)
		free(audio_configFormat);
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

int isCurrentAudioFormat(const struct audio_format *audioFormat)
{
	assert(audioFormat != NULL);

	return audio_format_equals(audioFormat, &audio_format);
}

static void syncAudioDeviceStates(void)
{
	struct audio_output *audioOutput;
	unsigned int i;

	if (!audio_format_defined(&audio_format))
		return;

	for (i = 0; i < audioOutputArraySize; ++i) {
		audioOutput = &audioOutputArray[i];
		switch (audioDeviceStates[i]) {
		case DEVICE_OFF:
			break;
		case DEVICE_ON:
			/* This will reopen only if the audio format changed */
			if (audio_output_open(audioOutput, &audio_format) < 0)
				audioDeviceStates[i] = DEVICE_ENABLE;
			break;
		case DEVICE_ENABLE:
			if (audio_output_open(audioOutput, &audio_format) == 0)
				audioDeviceStates[i] = DEVICE_ON;
			break;
		case DEVICE_DISABLE:
			audio_output_cancel(audioOutput);
			audio_output_close(audioOutput);
			audioDeviceStates[i] = DEVICE_OFF;
		}
	}
}

static int flushAudioBuffer(void)
{
	int ret = -1, err;
	unsigned int i;

	if (audioBufferPos == 0)
		return 0;

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audioDeviceStates[i] != DEVICE_ON)
			continue;
		err = audio_output_play(&audioOutputArray[i], audioBuffer,
					audioBufferPos);
		if (!err)
			ret = 0;
		else if (err < 0)
			/* device should already be closed if the play
			 * func returned an error */
			audioDeviceStates[i] = DEVICE_ENABLE;
	}

	audioBufferPos = 0;

	return ret;
}

int openAudioDevice(const struct audio_format *audioFormat)
{
	int ret = -1;
	unsigned int i;

	if (!audioOutputArray)
		return -1;

	if (!audioOpened ||
	    (audioFormat != NULL && !isCurrentAudioFormat(audioFormat))) {
		flushAudioBuffer();
		if (audioFormat != NULL)
			audio_format = *audioFormat;
		audioBufferSize = (audio_format.bits >> 3) *
		    audio_format.channels;
		audioBufferSize *= audio_format.sampleRate >> 5;
		audioBuffer = xrealloc(audioBuffer, audioBufferSize);
	}

	syncAudioDeviceStates();

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audioOutputArray[i].open)
			ret = 0;
	}

	if (ret == 0)
		audioOpened = 1;
	else {
		/* close all devices if there was an error */
		for (i = 0; i < audioOutputArraySize; ++i) {
			audio_output_close(&audioOutputArray[i]);
		}

		audioOpened = 0;
	}

	return ret;
}

int playAudio(const char *playChunk, size_t size)
{
	size_t send_size;

	while (size > 0) {
		send_size = audioBufferSize - audioBufferPos;
		send_size = send_size < size ? send_size : size;

		memcpy(audioBuffer + audioBufferPos, playChunk, send_size);
		audioBufferPos += send_size;
		size -= send_size;
		playChunk += send_size;

		if (audioBufferPos == audioBufferSize) {
			if (flushAudioBuffer() < 0)
				return -1;
		}
	}

	return 0;
}

void dropBufferedAudio(void)
{
	unsigned int i;

	syncAudioDeviceStates();
	audioBufferPos = 0;

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audioDeviceStates[i] == DEVICE_ON)
			audio_output_cancel(&audioOutputArray[i]);
	}
}

void closeAudioDevice(void)
{
	unsigned int i;

	flushAudioBuffer();

	free(audioBuffer);
	audioBuffer = NULL;
	audioBufferSize = 0;

	for (i = 0; i < audioOutputArraySize; ++i) {
		if (audioDeviceStates[i] == DEVICE_ON)
			audioDeviceStates[i] = DEVICE_ENABLE;
		audio_output_close(&audioOutputArray[i]);
	}

	audioOpened = 0;
}

void sendMetadataToAudioDevice(const struct tag *tag)
{
	unsigned int i;

	for (i = 0; i < audioOutputArraySize; ++i) {
		audio_output_send_tag(&audioOutputArray[i], tag);
	}
}

int enableAudioDevice(unsigned int device)
{
	if (device >= audioOutputArraySize)
		return -1;

	if (!(audioDeviceStates[device] & 0x01))
		audioDeviceStates[device] = DEVICE_ENABLE;

	return 0;
}

int disableAudioDevice(unsigned int device)
{
	if (device >= audioOutputArraySize)
		return -1;

	if (audioDeviceStates[device] & 0x01)
		audioDeviceStates[device] = DEVICE_DISABLE;

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
			      audioDeviceStates[i] & 0x01);
	}
}

void saveAudioDevicesState(FILE *fp)
{
	unsigned int i;

	assert(audioOutputArraySize != 0);
	for (i = 0; i < audioOutputArraySize; i++) {
		fprintf(fp, AUDIO_DEVICE_STATE "%d:%s\n",
			audioDeviceStates[i] & 0x01,
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

		if (strncmp(buffer, AUDIO_DEVICE_STATE, AUDIO_DEVICE_STATE_LEN))
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
					audioDeviceStates[i] = DEVICE_DISABLE;
				break;
			}
		}
		continue;
errline:
		/* nonfatal */
		ERROR("invalid line in state_file: %s\n", buffer);
	}
}

