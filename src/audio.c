/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

static AudioFormat audio_format;

static AudioFormat * audio_configFormat = NULL;

static AudioOutput ** audioOutputArray = NULL;
static mpd_uint8 audioOutputArraySize = 0;
/* the audioEnabledArray should be stuck into shared memory, and then disable
   and enable in playAudio() routine */
static mpd_uint8 * audioEnabledArray = NULL;

static mpd_uint8 audioOpened = 0;

void copyAudioFormat(AudioFormat * dest, AudioFormat * src) {
	if(!src) return;

	memcpy(dest, src, sizeof(AudioFormat));
}

int cmpAudioFormat(AudioFormat * f1, AudioFormat * f2) {
	return memcmp(f1, f2, sizeof(AudioFormat));
}

extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin shoutPlugin;
extern AudioOutputPlugin ossPlugin;

void initAudioDriver() {
	ConfigParam * param = NULL;
	int i;

	initAudioOutputPlugins();
	loadAudioOutputPlugin(&aoPlugin);
	loadAudioOutputPlugin(&shoutPlugin);
	loadAudioOutputPlugin(&ossPlugin);

	while((param = getNextConfigParam(CONF_AUDIO_OUTPUT, param))) {
		if(audioOutputArraySize == 255) {
			ERROR("only up to 255 audio output devices are "
					"supported");
			exit(EXIT_FAILURE);
		}

		i = audioOutputArraySize++;

		audioOutputArray = realloc(audioOutputArray,
				audioOutputArraySize*sizeof(AudioOutput *));
		audioEnabledArray = realloc(audioEnabledArray,
				audioOutputArraySize*sizeof(mpd_uint8));
	
		audioOutputArray[i] = newAudioOutput(param);
		audioEnabledArray[i] = 1;

		if(!audioOutputArray[i]) {
			ERROR("problems configuring output device defined at "
					"line %i\n", param->line);
			exit(EXIT_FAILURE);
		}
	}
}

void getOutputAudioFormat(AudioFormat * inAudioFormat, 
                AudioFormat * outAudioFormat)
{
        if(audio_configFormat) {
                copyAudioFormat(outAudioFormat,audio_configFormat);
        }
        else copyAudioFormat(outAudioFormat,inAudioFormat);
}

void initAudioConfig() {
        ConfigParam * param = getConfigParam(CONF_AUDIO_OUTPUT_FORMAT);

        if(NULL == param || NULL == param->value) return;

        audio_configFormat = malloc(sizeof(AudioFormat));

	if(0 != parseAudioConfig(audio_configFormat, param->value)) {
		ERROR("error parsing \"%s\" at line %i\n", 
				CONF_AUDIO_OUTPUT_FORMAT, param->line);
		exit(EXIT_FAILURE);
	}
}

int parseAudioConfig(AudioFormat * audioFormat, char * conf) {
        char * test;

        memset(audioFormat,0,sizeof(AudioFormat));

        audioFormat->sampleRate = strtol(conf,&test,10);
       
        if(*test!=':') {
                ERROR("error parsing audio output format: %s\n",conf);
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
        }*/

        if(audioFormat->sampleRate <= 0) {
                ERROR("sample rate %i is not >= 0\n",
                                (int)audioFormat->sampleRate);
		return -1;
        }

        audioFormat->bits = strtol(test+1,&test,10);
        
        if(*test!=':') {
                ERROR("error parsing audio output format: %s\n",conf);
		return -1;
        }

        switch(audioFormat->bits) {
        case 16:
                break;
        default:
                ERROR("bits %i can not be used for audio output\n",
                        (int)audioFormat->bits);
		return -1;
        }

        audioFormat->channels = strtol(test+1,&test,10);
        
        if(*test!='\0') {
                ERROR("error parsing audio output format: %s\n",conf);
		return -1;
        }

        switch(audioFormat->channels) {
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

void finishAudioConfig() {
        if(audio_configFormat) free(audio_configFormat);
}

void finishAudioDriver() {
	int i;

	for(i = 0; i < audioOutputArraySize; i++) {
		finishAudioOutput(audioOutputArray[i]);
	}

	free(audioEnabledArray);
	free(audioOutputArray);
	audioOutputArray = NULL;
	audioOutputArraySize = 0;
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
	if(!audioFormat) return 1;

	if(cmpAudioFormat(audioFormat, &audio_format) != 0) return 0;

	return 1;
}

int openAudioDevice(AudioFormat * audioFormat) {
	int isCurrentFormat = isCurrentAudioFormat(audioFormat);
	int ret = -1;
	int i;

	if(!audioOutputArray) return -1;

	if(!isCurrentFormat) {
		copyAudioFormat(&audio_format, audioFormat);
	}

	for(i = 0; i < audioOutputArraySize; i++) {
		if(!audioEnabledArray[i]) continue;
		if(!audioOutputArray[i]->open || !isCurrentFormat) {
			openAudioOutput(audioOutputArray[i], &audio_format);
		}
		if(audioOutputArray[i]->open) ret = 0;
	}

	if(ret == 0) audioOpened = 1;
	else {
		/* close all devices if there was an error */
		for(i = 0; i < audioOutputArraySize; i++) {
			closeAudioOutput(audioOutputArray[i]);
		}

		audioOpened = 0;
	}

	return ret;
}

int playAudio(char * playChunk, int size) {
	int ret = -1;
	int i;

	/* put some here to determine if enabled array changed */

	for(i = 0; i < audioOutputArraySize; i++) {
		if(!audioEnabledArray[i]) continue;
		if(0 == playAudioOutput(audioOutputArray[i], playChunk, size)) {
			ret = 0;
		}
	}

	return ret;
}

int isAudioDeviceOpen() {
	/*int ret = 0;
	int i;

	for(i = 0; i < audioOutputArraySize; i++) {
		if(!audioEnabledArray[i]) continue;
		ret |= audioOutputArray[i]->open;
	}*/

	return audioOpened;
}

void closeAudioDevice() {
	int i;

	for(i = 0; i < audioOutputArraySize; i++) {
		closeAudioOutput(audioOutputArray[i]);
	}

	audioOpened = 0;
}

void sendMetadataToAudioDevice(MpdTag * tag) {
	int i;

	for(i = 0; i < audioOutputArraySize; i++) {
		sendMetadataToAudioOutput(audioOutputArray[i], tag);
	}
}

int enableAudioDevice(FILE * fp, int device) {
	if(device < 0 || device >= audioOutputArraySize) {
		commandError(fp, ACK_ERROR_ARG, "audio output device id %i "
				"doesn't exist\n", device);
		return -1;
	}

	audioEnabledArray[device] = 1;
	/*if(audioOpened && !audioOutputArray[device]->open) {
		openAudioOutput(audioOutputArray[device], &audio_format);
	}*/

	return 0;
}

int disableAudioDevice(FILE * fp, int device) {
	if(device < 0 || device >= audioOutputArraySize) {
		commandError(fp, ACK_ERROR_ARG, "audio output device id %i "
				"doesn't exist\n", device);
		return -1;
	}

	audioEnabledArray[device] = 0;
	/*closeAudioOutput(audioOutputArray[device]);*/

	return 0;
}
