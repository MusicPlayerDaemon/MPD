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

#include "audioOutput.h"
#include "conf.h"
#include "log.h"
#include "sig_handlers.h"

#include <string.h>
#include <assert.h>
#include <signal.h>

#include <ao/ao.h>

static int audioOutputAo_write_size;

static int audioOutputAo_driver_id;
static ao_option * audioOutputAo_options;

static ao_device * audioOutputAo_device = NULL;

void audioOutputAo_initDriver() {
	ao_info * ai;
	char * dup;
	char * stk1;
	char * stk2;
	char * n1;
	char * key;
	char * value;
	char * test;

	audio_write_size = strtol((getConf())[CONF_AUDIO_WRITE_SIZE],&test,10);
	if (*test!='\0') {
		ERROR("\"%s\" is not a valid write size",
			(getConf())[CONF_AUDIO_WRITE_SIZE]);
		exit(EXIT_FAILURE);
	}

	audio_ao_options = NULL;

	ao_initialize();
	if(strcmp(AUDIO_AO_DRIVER_DEFAULT,(getConf())[CONF_AO_DRIVER])==0) {
		audio_ao_driver_id = ao_default_driver_id();
	}
	else if((audio_ao_driver_id = 
		ao_driver_id((getConf())[CONF_AO_DRIVER]))<0) {
		ERROR("\"%s\" is not a valid ao driver\n",
			(getConf())[CONF_AO_DRIVER]);
		exit(EXIT_FAILURE);
	}
	
	if((ai = ao_driver_info(audio_ao_driver_id))==NULL) {
		ERROR("problems getting ao_driver_info\n");
		ERROR("you may not have permission to the audio device\n");
		exit(EXIT_FAILURE);
	}

	dup = strdup((getConf())[CONF_AO_DRIVER_OPTIONS]);
	if(strlen(dup)) {
		stk1 = NULL;
		n1 = strtok_r(dup,";",&stk1);
		while(n1) {
			stk2 = NULL;
			key = strtok_r(n1,"=",&stk2);
			if(!key) {
				ERROR("problems parsing "
					"ao_driver_options \"%s\"\n", n1);
				exit(EXIT_FAILURE);
			}
			/*found = 0;
			for(i=0;i<ai->option_count;i++) {
				if(strcmp(ai->options[i],key)==0) {
					found = 1;
					break;
				}
			}
			if(!found) {
				ERROR("\"%s\" is not an option for "
					 "\"%s\" ao driver\n",key,
					 ai->short_name);
				exit(EXIT_FAILURE);
			}*/
			value = strtok_r(NULL,"",&stk2);
			if(!value) {
				ERROR("problems parsing "
					"ao_driver_options \"%s\"\n", n1);
				exit(EXIT_FAILURE);
			}
			ao_append_option(&audio_ao_options,key,value);
			n1 = strtok_r(NULL,";",&stk1);
		}
	}
	free(dup);
}

void audioOutputAo_initConfig() {
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

void audioOutputAo_finishConfig() {
        if(audio_configFormat) free(audio_configFormat);
}

void audioOutputAo_finishDriver() {
	ao_free_options(audio_ao_options);

	ao_shutdown();
}

int audioOutputAo_openDevice(AudioFormat * audioFormat) {
	ao_sample_format format;

	if(audio_device && !isCurrentAudioFormat(audioFormat)) {
		closeAudioDevice();
	}

	if(!audio_device) {
		if(audioFormat) {
                        copyAudioFormat(&audio_format,audioFormat);
		}

		format.bits = audio_format.bits;
		format.rate = audio_format.sampleRate;
		format.byte_format = AO_FMT_NATIVE;
		format.channels = audio_format.channels;

		blockSignals();
		audio_device = ao_open_live(audio_ao_driver_id, &format, 
					audio_ao_options);
		unblockSignals();

		if(audio_device==NULL) return -1;
	}

	return 0;
}


int audioOutputAo_playAudio(char * playChunk, int size) {
	int send;

	if(audio_device==NULL) {
		ERROR("trying to play w/o the audio device being open!\n");
		return -1;
	}
	
	while(size>0) {
		send = audio_write_size>size?size:audio_write_size;
		
		if(ao_play(audio_device,playChunk,send)==0) {
			audioError();
			ERROR("closing audio device due to write error\n");
			closeAudioDevice();
			return -1;
		}

		playChunk+=send;
		size-=send;
	}

	return 0;
}

void audioOutputAo_closeAudioDevice() {
	if(audio_device) {
		blockSignals();
		ao_close(audio_device);
		audio_device = NULL;
		unblockSignals();
	}
}

AudioOutputPlugin aoPlugin = 
{
	"ao",
	audioOutputAo_initConfig,
	audioOutputAo_finishConfig,
	audioOutputAo_initDriver,
	audioOutputAo_finishDriver,
	audioOutputAo_openDevice,
	audioOutputAo_play,
	audioOutputAo_closeDevice
};
