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
#include "playerData.h"
#include "utils.h"

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
static void saveAudioDevicesState();

static AudioFormat audio_format;

static AudioFormat * audio_configFormat = NULL;

static AudioOutput ** audioOutputArray = NULL;
static mpd_uint8 audioOutputArraySize = 0;
/* the audioEnabledArray should be stuck into shared memory, and then disable
   and enable in playAudio() routine */
static mpd_sint8 * pdAudioDevicesEnabled = NULL;
static mpd_sint8 myAudioDevicesEnabled[AUDIO_MAX_DEVICES];

static mpd_uint8 audioOpened = 0;

static mpd_sint32 audioBufferSize = 0;
static char * audioBuffer = NULL;
static mpd_sint32 audioBufferPos = 0;

void copyAudioFormat(AudioFormat * dest, AudioFormat * src) {
	if(!src) return;

	memcpy(dest, src, sizeof(AudioFormat));
}

int cmpAudioFormat(AudioFormat * f1, AudioFormat * f2)
{
	if (f1 && f2 && (f1->sampleRate == f2->sampleRate) &&
			(f1->bits == f2->bits) &&
			(f1->channels == f2->channels))
		return 0;
	return 1;
}

extern AudioOutputPlugin alsaPlugin;
extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin ossPlugin;
extern AudioOutputPlugin osxPlugin;
extern AudioOutputPlugin mvpPlugin;
extern AudioOutputPlugin shoutPlugin;

/* make sure initPlayerData is called before this function!! */
void initAudioDriver() {
	ConfigParam * param = NULL;
	int i;

	initAudioOutputPlugins();
	loadAudioOutputPlugin(&alsaPlugin);
	loadAudioOutputPlugin(&aoPlugin);
	loadAudioOutputPlugin(&ossPlugin);
	loadAudioOutputPlugin(&osxPlugin);
	loadAudioOutputPlugin(&mvpPlugin);
	loadAudioOutputPlugin(&shoutPlugin);

	pdAudioDevicesEnabled = (getPlayerData())->audioDeviceEnabled;

	for(i = 0; i < AUDIO_MAX_DEVICES; i++) {
		pdAudioDevicesEnabled[i] = 1;
		myAudioDevicesEnabled[i] = 1;
	}

	param = getNextConfigParam(CONF_AUDIO_OUTPUT, param);

	do {
		AudioOutput *output;
		int j;

		if(audioOutputArraySize == AUDIO_MAX_DEVICES) {
			ERROR("only up to 255 audio output devices are "
					"supported");
			exit(EXIT_FAILURE);
		}

		i = audioOutputArraySize++;

		audioOutputArray = realloc(audioOutputArray,
				audioOutputArraySize*sizeof(AudioOutput *));

		output = newAudioOutput(param);
		if(!output && param) {
			ERROR("problems configuring output device defined at "
					"line %i\n", param->line);
			exit(EXIT_FAILURE);
		}

		/* require output names to be unique: */
		for (j = i - 1; j >= 0; --j) {
			if ( !strcmp( output->name,
					audioOutputArray[j]->name) ) {
				ERROR("output devices with identical "
						"names: %s\n",
						output->name);
				exit(EXIT_FAILURE);
			}
		}
		audioOutputArray[i] = output;
	} while((param = getNextConfigParam(CONF_AUDIO_OUTPUT, param)));
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

	saveAudioDevicesState();
	for(i = 0; i < audioOutputArraySize; i++) {
		finishAudioOutput(audioOutputArray[i]);
	}

	free(audioOutputArray);
	audioOutputArray = NULL;
	audioOutputArraySize = 0;
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
	if(!audioFormat) return 1;

	if(cmpAudioFormat(audioFormat, &audio_format) != 0) return 0;

	return 1;
}

inline void syncAudioDevicesEnabledArrays() {
	int i;

	memcpy(myAudioDevicesEnabled, pdAudioDevicesEnabled,AUDIO_MAX_DEVICES);
			
	for(i = 0; i < audioOutputArraySize; i++) {
		if(myAudioDevicesEnabled[i]) {
			openAudioOutput(audioOutputArray[i], &audio_format);
		}
		else {
			dropBufferedAudioOutput(audioOutputArray[i]);
			closeAudioOutput(audioOutputArray[i]);
		}
	}
}

static int flushAudioBuffer() {
	int ret = -1;
	int i;

	if(audioBufferPos == 0) return 0;

	if(0 != memcmp(pdAudioDevicesEnabled, myAudioDevicesEnabled,
			AUDIO_MAX_DEVICES)) 
	{
		syncAudioDevicesEnabledArrays();
	}

	for(i = 0; i < audioOutputArraySize; i++) {
		if(!myAudioDevicesEnabled[i]) continue;
		if(0 == playAudioOutput(audioOutputArray[i], audioBuffer, 
					audioBufferPos)) 
		{
			ret = 0;
		}
	}

	audioBufferPos = 0;

	return ret;
}

int openAudioDevice(AudioFormat * audioFormat) {
	int isCurrentFormat = isCurrentAudioFormat(audioFormat);
	int ret = -1;
	int i;

	if(!audioOutputArray) return -1;

	if(!audioOpened || !isCurrentFormat) {
		flushAudioBuffer();
		copyAudioFormat(&audio_format, audioFormat);
		audioBufferSize = (audio_format.bits >> 3)*
					audio_format.channels;
		audioBufferSize*= audio_format.sampleRate >> 5;
		audioBuffer = realloc(audioBuffer, audioBufferSize);
	}

	syncAudioDevicesEnabledArrays();
	
	for(i = 0; i < audioOutputArraySize; i++) {
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
	int send;
	
	while(size > 0) {
		send = audioBufferSize-audioBufferPos;
		send = send < size ? send : size;

		memcpy(audioBuffer+audioBufferPos, playChunk, send);
		audioBufferPos += send;
		size -= send;
		playChunk+= send;

		if(audioBufferPos == audioBufferSize) {
			if( flushAudioBuffer() < 0 ) return -1;
		}
	}

	return 0;
}

int isAudioDeviceOpen() {
	return audioOpened;
}

void dropBufferedAudio() {
	int i;

	if(0 != memcmp(pdAudioDevicesEnabled, myAudioDevicesEnabled,
			AUDIO_MAX_DEVICES)) 
	{
		syncAudioDevicesEnabledArrays();
	}

	audioBufferPos = 0;

	for(i = 0; i < audioOutputArraySize; i++) {
		if(!myAudioDevicesEnabled[i]) continue;
		dropBufferedAudioOutput(audioOutputArray[i]);
	}
}

void closeAudioDevice() {
	int i;

	flushAudioBuffer();

	free(audioBuffer);
	audioBuffer = NULL;
	audioBufferSize = 0;

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

	pdAudioDevicesEnabled[device] = 1;

	return 0;
}

int disableAudioDevice(FILE * fp, int device) {
	if(device < 0 || device >= audioOutputArraySize) {
		commandError(fp, ACK_ERROR_ARG, "audio output device id %i "
				"doesn't exist\n", device);
		return -1;
	}

	pdAudioDevicesEnabled[device] = 0;

	return 0;
}

void printAudioDevices(FILE * fp) {
	int i;

	for(i = 0; i < audioOutputArraySize; i++) {
		myfprintf(fp, "outputid: %i\n", i);
		myfprintf(fp, "outputname: %s\n", audioOutputArray[i]->name);
		myfprintf(fp, "outputenabled: %i\n", 
				(int)pdAudioDevicesEnabled[i]);
	}
}

static char * getStateFile() {
	ConfigParam * param = parseConfigFilePath(CONF_STATE_FILE, 0);

	if(!param) return NULL;

	return param->value;
}

static void saveAudioDevicesState() {
	char *stateFile;
	FILE *fp;
	int i;

	if (!(stateFile = getStateFile()))
		return;

	while(!(fp = fopen(stateFile,"a")) && errno==EINTR);
	if(!fp) {
		ERROR("problems opening state file \"%s\" for "
			"writing: %s\n", stateFile, strerror(errno));
		return;
	}

	assert(audioOutputArraySize != 0);
	for (i = audioOutputArraySize - 1; i >= 0; --i) {
		myfprintf(fp, AUDIO_DEVICE_STATE "%d:%s\n",
					(int)pdAudioDevicesEnabled[i],
					audioOutputArray[i]->name);
	}
	while(fclose(fp) && errno==EINTR);
}

static void parse_audio_device_state(FILE *fp)
{
	char buffer[AUDIO_BUFFER_SIZE];
	int i;

	assert(audioOutputArraySize != 0);

	while (myFgets(buffer,AUDIO_BUFFER_SIZE,fp)) {
		char *c, *name;

		if (strncmp(buffer,AUDIO_DEVICE_STATE,AUDIO_DEVICE_STATE_LEN))
			continue;

		c = strchr(buffer,':');
		if (!c || !(++c))
			goto errline;

		name = strchr(c,':');
		if (!name || !(++name))
			goto errline;

		for (i = audioOutputArraySize - 1; i >= 0; --i) {
			if (!strcmp(name, audioOutputArray[i]->name)) {
				pdAudioDevicesEnabled[i] = atoi(c);
				break;
			}
		}
		continue;
errline:
		/* nonfatal */
		ERROR("invalid line in state_file: %s\n", buffer);
	}
}

void readAudioDevicesState() {
	char *stateFile;
	FILE *fp;
	struct stat st;

	if (!(stateFile = getStateFile()))
		return;
	if(stat(stateFile,&st)<0) {
		DEBUG("failed to stat state file\n");
		return;
	}
	if(!S_ISREG(st.st_mode)) {
		ERROR("state file \"%s\" is not a regular file\n",
							stateFile);
		exit(EXIT_FAILURE);
	}

	fp = fopen(stateFile,"r");
	if(!fp) {
		ERROR("problems opening state file \"%s\" for "
			"reading: %s\n", stateFile,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	parse_audio_device_state(fp);
	fclose(fp);
}

