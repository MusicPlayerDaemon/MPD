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

static void copyAudioFormat(AudioFormat * dest, AudioFormat * src) {
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
        char * test;

        if(NULL == conf) return;

        audio_configFormat = malloc(sizeof(AudioFormat));

        memset(audio_configFormat,0,sizeof(AudioFormat));

        audio_configFormat->sampleRate = strtol(conf,&test,10);
       
        if(*test!=':') {
                ERROR("error parsing audio output format: %s\n",conf);
                exit(EXIT_FAILURE);
        }
 
        /*switch(audio_configFormat->sampleRate) {
        case 48000:
        case 44100:
        case 32000:
        case 16000:
                break;
        default:
                ERROR("sample rate %i can not be used for audio output\n",
                        (int)audio_configFormat->sampleRate);
                exit(EXIT_FAILURE);
        }*/

        if(audio_configFormat->sampleRate <= 0) {
                ERROR("sample rate %i is not >= 0\n",
                                (int)audio_configFormat->sampleRate);
                exit(EXIT_FAILURE);
        }

        audio_configFormat->bits = strtol(test+1,&test,10);
        
        if(*test!=':') {
                ERROR("error parsing audio output format: %s\n",conf);
                exit(EXIT_FAILURE);
        }

        switch(audio_configFormat->bits) {
        case 16:
                break;
        default:
                ERROR("bits %i can not be used for audio output\n",
                        (int)audio_configFormat->bits);
                exit(EXIT_FAILURE);
        }

        audio_configFormat->channels = strtol(test+1,&test,10);
        
        if(*test!='\0') {
                ERROR("error parsing audio output format: %s\n",conf);
                exit(EXIT_FAILURE);
        }

        switch(audio_configFormat->channels) {
        case 2:
                break;
        default:
                ERROR("channels %i can not be used for audio output\n",
                        (int)audio_configFormat->channels);
                exit(EXIT_FAILURE);
        }
}

void finishAudioConfig() {
        if(audio_configFormat) free(audio_configFormat);
}

void finishAudioDriver() {
	finishAudioOutput(aoOutput);
	if(shoutOutput) finishAudioOutput(shoutOutput);
	aoOutput = NULL;
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
	if(!audioFormat) return 0;

	if(memcmp(audioFormat,&audio_format,sizeof(AudioFormat)) != 0) return 0;

	return 1;
}

int openAudioDevice(AudioFormat * audioFormat) {
	if(!aoOutput->open || !isCurrentAudioFormat(audioFormat)) {
		copyAudioFormat(&audio_format, audioFormat);
		if(shoutOutput) openAudioOutput(shoutOutput, audioFormat);
		return openAudioOutput(aoOutput, audioFormat);
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
