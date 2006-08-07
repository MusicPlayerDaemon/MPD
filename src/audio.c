/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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
#include "audioOutput.h"
#include "conf.h"
#include "log.h"
#include "sig_handlers.h"
#include "command.h"
#include "playerData.h"
#include "utils.h"
#include "playlist.h"
#include "state_file.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>

#define AUDIO_DEVICE_STATE	"audio_device_state:"
#define AUDIO_DEVICE_STATE_LEN	19	/* strlen(AUDIO_DEVICE_STATE) */
#define AUDIO_BUFFER_SIZE	2*MAXPATHLEN

static AudioFormat audio_format = { 0, 0, 0 };

static AudioFormat *audio_configFormat = NULL;

static AudioOutput *audioOutputArray = NULL;
static mpd_uint8 audioOutputArraySize = 0;

#define	DEVICE_OFF        0x00
#define DEVICE_ENABLE	  0x01   /* currently off, but to be turned on */
#define	DEVICE_ON         0x03
#define DEVICE_DISABLE    0x04   /* currently on, but to be turned off */

/* the audioEnabledArray should be stuck into shared memory, and then disable
   and enable in playAudio() routine */
static mpd_uint8 *audioDeviceStates = NULL;

static mpd_uint8 audioOpened = 0;

static mpd_sint32 audioBufferSize = 0;
static char *audioBuffer = NULL;
static mpd_sint32 audioBufferPos = 0;

size_t audio_device_count(void)
{
	size_t nr = 0;
	ConfigParam *param = NULL;

	while ((param = getNextConfigParam(CONF_AUDIO_OUTPUT, param)))
		nr++;
	if (!nr)
		nr = 1; /* we'll always have at least one device  */
	return nr;
}

void copyAudioFormat(AudioFormat * dest, AudioFormat * src)
{
	if (!src)
		return;

	memcpy(dest, src, sizeof(AudioFormat));
}

int cmpAudioFormat(AudioFormat * f1, AudioFormat * f2)
{
	if (f1 && f2 && (0 == memcmp(f1, f2, sizeof(AudioFormat))))
		return 0;
	return 1;
}

extern AudioOutputPlugin alsaPlugin;
extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin ossPlugin;
extern AudioOutputPlugin osxPlugin;
extern AudioOutputPlugin pulsePlugin;
extern AudioOutputPlugin mvpPlugin;
extern AudioOutputPlugin shoutPlugin;

void loadAudioDrivers(void)
{
	initAudioOutputPlugins();
	loadAudioOutputPlugin(&alsaPlugin);
	loadAudioOutputPlugin(&aoPlugin);
	loadAudioOutputPlugin(&ossPlugin);
	loadAudioOutputPlugin(&osxPlugin);
	loadAudioOutputPlugin(&pulsePlugin);
	loadAudioOutputPlugin(&mvpPlugin);
	loadAudioOutputPlugin(&shoutPlugin);
}

/* make sure initPlayerData is called before this function!! */
void initAudioDriver(void)
{
	ConfigParam *param = NULL;
	int i;

	loadAudioDrivers();

	audioOutputArraySize = audio_device_count();
	audioDeviceStates = (getPlayerData())->audioDeviceStates;
	audioOutputArray = malloc(sizeof(AudioOutput) * audioOutputArraySize);

	i = 0;
	param = getNextConfigParam(CONF_AUDIO_OUTPUT, param);

	do {
		AudioOutput *output = &audioOutputArray[i];
		int j;

		if (!initAudioOutput(output, param) && param) {
			ERROR("problems configuring output device defined at "
			      "line %i\n", param->line);
			exit(EXIT_FAILURE);
		}

		/* require output names to be unique: */
		for (j = i; --j >= 0; ) {
			if (!strcmp(output->name, audioOutputArray[j].name)) {
				ERROR("output devices with identical "
				      "names: %s\n", output->name);
				exit(EXIT_FAILURE);
			}
		}
		audioDeviceStates[i++] = DEVICE_ENABLE;
	} while ((param = getNextConfigParam(CONF_AUDIO_OUTPUT, param)));
}

void getOutputAudioFormat(AudioFormat * inAudioFormat,
			  AudioFormat * outAudioFormat)
{
	if (audio_configFormat) {
		copyAudioFormat(outAudioFormat, audio_configFormat);
	} else
		copyAudioFormat(outAudioFormat, inAudioFormat);
}

void initAudioConfig(void)
{
	ConfigParam *param = getConfigParam(CONF_AUDIO_OUTPUT_FORMAT);

	if (NULL == param || NULL == param->value)
		return;

	audio_configFormat = malloc(sizeof(AudioFormat));

	if (0 != parseAudioConfig(audio_configFormat, param->value)) {
		ERROR("error parsing \"%s\" at line %i\n",
		      CONF_AUDIO_OUTPUT_FORMAT, param->line);
		exit(EXIT_FAILURE);
	}
}

int parseAudioConfig(AudioFormat * audioFormat, char *conf)
{
	char *test;

	memset(audioFormat, 0, sizeof(AudioFormat));

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

	audioFormat->bits = strtol(test + 1, &test, 10);

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

	audioFormat->channels = strtol(test + 1, &test, 10);

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
	int i;

	for (i = 0; i < audioOutputArraySize; i++) {
		finishAudioOutput(&audioOutputArray[i]);
	}

	free(audioOutputArray);
	audioOutputArray = NULL;
	audioOutputArraySize = 0;
}

int isCurrentAudioFormat(AudioFormat * audioFormat)
{
	if (!audioFormat)
		return 1;

	if (cmpAudioFormat(audioFormat, &audio_format) != 0)
		return 0;

	return 1;
}

static void syncAudioDeviceStates(void)
{
	int i;

	if (!audio_format.channels)
		return;
	for (i = audioOutputArraySize; --i >= 0; ) {
		switch (audioDeviceStates[i]) {
		case DEVICE_ON:
			/* This will reopen only if the audio format changed */
			openAudioOutput(&audioOutputArray[i], &audio_format);
			break;
		case DEVICE_ENABLE:
			openAudioOutput(&audioOutputArray[i], &audio_format);
			audioDeviceStates[i] = DEVICE_ON;
			break;
		case DEVICE_DISABLE:
			dropBufferedAudioOutput(&audioOutputArray[i]);
			closeAudioOutput(&audioOutputArray[i]);
			audioDeviceStates[i] = DEVICE_OFF;
			break;
		}
	}
}

static int flushAudioBuffer(void)
{
	int ret = -1;
	int i, err;

	if (audioBufferPos == 0)
		return 0;

	syncAudioDeviceStates();

	for (i = audioOutputArraySize; --i >= 0; ) {
		if (audioDeviceStates[i] != DEVICE_ON)
			continue;
		err = playAudioOutput(&audioOutputArray[i], audioBuffer,
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

int openAudioDevice(AudioFormat * audioFormat)
{
	int isCurrentFormat = isCurrentAudioFormat(audioFormat);
	int ret = -1;
	int i;

	if (!audioOutputArray)
		return -1;

	if (!audioOpened || !isCurrentFormat) {
		flushAudioBuffer();
		copyAudioFormat(&audio_format, audioFormat);
		audioBufferSize = (audio_format.bits >> 3) *
		    audio_format.channels;
		audioBufferSize *= audio_format.sampleRate >> 5;
		audioBuffer = realloc(audioBuffer, audioBufferSize);
	}

	syncAudioDeviceStates();

	for (i = audioOutputArraySize; --i >= 0; ) {
		if (audioOutputArray[i].open)
			ret = 0;
	}

	if (ret == 0)
		audioOpened = 1;
	else {
		/* close all devices if there was an error */
		for (i = audioOutputArraySize; --i >= 0; ) {
			closeAudioOutput(&audioOutputArray[i]);
		}

		audioOpened = 0;
	}

	return ret;
}

int playAudio(char *playChunk, int size)
{
	int send;

	while (size > 0) {
		send = audioBufferSize - audioBufferPos;
		send = send < size ? send : size;

		memcpy(audioBuffer + audioBufferPos, playChunk, send);
		audioBufferPos += send;
		size -= send;
		playChunk += send;

		if (audioBufferPos == audioBufferSize) {
			if (flushAudioBuffer() < 0)
				return -1;
		}
	}

	return 0;
}

int isAudioDeviceOpen(void)
{
	return audioOpened;
}

void dropBufferedAudio(void)
{
	int i;

	syncAudioDeviceStates();
	audioBufferPos = 0;

	for (i = audioOutputArraySize; --i >= 0; ) {
		if (audioDeviceStates[i] == DEVICE_ON)
			dropBufferedAudioOutput(&audioOutputArray[i]);
	}
}

void closeAudioDevice(void)
{
	int i;

	flushAudioBuffer();

	free(audioBuffer);
	audioBuffer = NULL;
	audioBufferSize = 0;

	for (i = audioOutputArraySize; --i >= 0; ) {
		if (audioDeviceStates[i] == DEVICE_ON)
			audioDeviceStates[i] = DEVICE_ENABLE;
		closeAudioOutput(&audioOutputArray[i]);
	}

	audioOpened = 0;
}

void sendMetadataToAudioDevice(MpdTag * tag)
{
	int i;

	for (i = audioOutputArraySize; --i >= 0; ) {
		sendMetadataToAudioOutput(&audioOutputArray[i], tag);
	}
}

int enableAudioDevice(int fd, int device)
{
	if (device < 0 || device >= audioOutputArraySize) {
		commandError(fd, ACK_ERROR_ARG, "audio output device id %i "
			     "doesn't exist\n", device);
		return -1;
	}

	if (!(audioDeviceStates[device] & 0x01))
		audioDeviceStates[device] = DEVICE_ENABLE;

	return 0;
}

int disableAudioDevice(int fd, int device)
{
	if (device < 0 || device >= audioOutputArraySize) {
		commandError(fd, ACK_ERROR_ARG, "audio output device id %i "
			     "doesn't exist\n", device);
		return -1;
	}
	if (audioDeviceStates[device] & 0x01)
		audioDeviceStates[device] = DEVICE_DISABLE;

	return 0;
}

void printAudioDevices(int fd)
{
	int i;

	for (i = 0; i < audioOutputArraySize; i++) {
		fdprintf(fd,
		         "outputid: %i\noutputname: %s\noutputenabled: %i\n",
			 i,
			 audioOutputArray[i].name,
			 audioDeviceStates[i] & 0x01);
	}
}

void saveAudioDevicesState(FILE *fp)
{
	int i;

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
	int i;

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

		for (i = audioOutputArraySize; --i >= 0; ) {
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

