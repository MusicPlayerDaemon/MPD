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

int audio_write_size;

int audio_ao_driver_id;
ao_option * audio_ao_options;

AudioFormat audio_format;
ao_device * audio_device = NULL;

void initAudioDriver() {
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
		exit(-1);
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
		exit(-1);
	}
	
	if((ai = ao_driver_info(audio_ao_driver_id))==NULL) {
		ERROR("problems getting ao_driver_info\n");
		ERROR("you may not have permission to the audio device\n");
		exit(-1);
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
				exit(-1);
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
				exit(-1);
			}*/
			value = strtok_r(NULL,"",&stk2);
			if(!value) {
				ERROR("problems parsing "
					"ao_driver_options \"%s\"\n", n1);
				exit(-1);
			}
			ao_append_option(&audio_ao_options,key,value);
			n1 = strtok_r(NULL,";",&stk1);
		}
	}
	free(dup);
}

void finishAudioDriver() {
	ao_free_options(audio_ao_options);

	ao_shutdown();
}

int isCurrentAudioFormat(AudioFormat * audioFormat) {
	if(!audio_device) return 0;

	if(audio_format.bits!=audioFormat->bits || 
			audio_format.sampleRate!=audioFormat->sampleRate ||
			audio_format.channels!=audioFormat->channels) 
	{
		return 0;
	}

	return 1;
}

int initAudio(AudioFormat * audioFormat) {
	ao_sample_format format;

	if(!isCurrentAudioFormat(audioFormat)) {
		finishAudio();
	}

	if(!audio_device) {
		format.bits = audioFormat->bits;
		format.rate = audioFormat->sampleRate;
		format.byte_format = AO_FMT_NATIVE;
		format.channels = audioFormat->channels;
		audio_format.bits = format.bits;
		audio_format.sampleRate = format.rate;
		audio_format.channels = format.channels;

		blockSignals();

		audio_device = ao_open_live(audio_ao_driver_id, &format, 
					audio_ao_options);

		if(audio_device==NULL) {
			unblockSignals();
			audioError();
			return -1;
		}
		unblockSignals();
	}

	return 0;
}


void playAudio(char * playChunk, int size) {
	int send;
	
	assert(audio_device!=NULL);

	while(size>0) {
		send = audio_write_size>size?size:audio_write_size;
		
		ao_play(audio_device,playChunk,send);

		playChunk+=send;
		size-=send;
	}
}

void finishAudio() {
	if(audio_device) {
		blockSignals();
		ao_close(audio_device);
		audio_device = NULL;
		unblockSignals();
	}
}

void audioError() {
	ERROR("Error opening audio device\n");
	if(errno==AO_ENOTLIVE) {
		ERROR("not a live ao device\n");
	}
	else if(errno==AO_EOPENDEVICE) {
		ERROR("not able to open audio device\n");
	}
	else if(errno==AO_EBADOPTION) {
		ERROR("bad driver option\n");
	}
}
