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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

static AudioFormat audio_format;

static AudioFormat * audio_configFormat = NULL;

static AudioOutput * aoOutput = NULL;
static AudioOutput * shoutOutput = NULL;

void copyAudioFormat(AudioFormat * dest, AudioFormat * src) {
	if(!src) return;

        dest->sampleRate = src->sampleRate;
        dest->bits = src->bits;
        dest->channels = src->channels;
}

extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin shoutPlugin;

void initAudioDriver() {
	initAudioOutputPlugins();
	loadAudioOutputPlugin(&aoPlugin);
	loadAudioOutputPlugin(&shoutPlugin);

	aoOutput = newAudioOutput("ao");
	assert(aoOutput);
	shoutOutput = newAudioOutput("shout");
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
        char * conf = getConf()[CONF_AUDIO_OUTPUT_FORMAT];

        if(NULL == conf) return;

        audio_configFormat = malloc(sizeof(AudioFormat));

	if(0 != parseAudioConfig(audio_configFormat, conf)) exit(EXIT_FAILURE);
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
	finishAudioOutput(aoOutput);
	if(shoutOutput) finishAudioOutput(shoutOutput);
	shoutOutput = NULL;
	aoOutput = NULL;
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
	if(!audioFormat) return 0;

	if(memcmp(audioFormat,&audio_format,sizeof(AudioFormat)) != 0) return 0;

	return 1;
}

int openAudioDevice(AudioFormat * audioFormat) {
	if(!aoOutput->open || !isCurrentAudioFormat(audioFormat)) {
		if(audioFormat) copyAudioFormat(&audio_format, audioFormat);
		if(shoutOutput) openAudioOutput(shoutOutput, &audio_format);
		return openAudioOutput(aoOutput, &audio_format);
	}

	return 0;
}

int playAudio(char * playChunk, int size) {
	if(shoutOutput) playAudioOutput(shoutOutput, playChunk, size);
	return playAudioOutput(aoOutput, playChunk, size);
}

int isAudioDeviceOpen() {
	return aoOutput->open;
}

void closeAudioDevice() {
	if(shoutOutput) closeAudioOutput(shoutOutput);
	closeAudioOutput(aoOutput);
}

void sendMetdataToAudioDevice(MpdTag * tag) {
	if(shoutOutput) sendMetadataToAudioOutput(shoutOutput, tag);
}
