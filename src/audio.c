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
#include "conf.h"
#include "log.h"
#include "sig_handlers.h"

#include <string.h>
#include <assert.h>
#include <signal.h>

#ifdef HAVE_AUDIO
#include <ao/ao.h>

static int audio_write_size;

static int audio_ao_driver_id;
static ao_option * audio_ao_options;

static AudioFormat audio_format;
static ao_device * audio_device = NULL;
#endif

static AudioFormat * audio_configFormat = NULL;

static void copyAudioFormat(AudioFormat * dest, AudioFormat * src) {
        dest->sampleRate = src->sampleRate;
        dest->bits = src->bits;
        dest->channels = src->channels;
}

void initAudioDriver() {
#ifdef HAVE_AUDIO
	ao_info * ai;
	char * dup;
	char * stk1;
	char * stk2;
	char * n1;
	char * key;
	char * value;
	char * test;
	/*int found;
	int i;*/

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
#endif
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
 
        switch(audio_configFormat->sampleRate) {
        case 48000:
        case 44100:
                break;
        default:
                ERROR("sample rate %i can not be used for audio output\n",
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
#ifdef HAVE_AUDIO
	ao_free_options(audio_ao_options);

	ao_shutdown();
#endif
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
#ifdef HAVE_AUDIO
	if(!audio_device || !audioFormat) return 0;

	if(memcmp(audioFormat,&audio_format,sizeof(AudioFormat)) != 0) return 0;
#endif
	return 1;
}

int openAudioDevice(AudioFormat * audioFormat) {
#ifdef HAVE_AUDIO
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
#endif
	return 0;
}


int playAudio(char * playChunk, int size) {
#ifdef HAVE_AUDIO
	int send;

	if(audio_device==NULL) {
		ERROR("trying to play w/ the audio device being open!\n");
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

#endif
	return 0;
}

void closeAudioDevice() {
#ifdef HAVE_AUDIO
	if(audio_device) {
		blockSignals();
		ao_close(audio_device);
		audio_device = NULL;
		unblockSignals();
	}
#endif
}

void audioError() {
#ifdef HAVE_AUDIO
	if(errno==AO_ENOTLIVE) {
		ERROR("not a live ao device\n");
	}
	else if(errno==AO_EOPENDEVICE) {
		ERROR("not able to open audio device\n");
	}
	else if(errno==AO_EBADOPTION) {
		ERROR("bad driver option\n");
	}
#endif
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
