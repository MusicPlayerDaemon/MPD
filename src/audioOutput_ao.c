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

static int driverInitCount = 0;

typedef struct _AoData {
	int writeSize;
	int driverId;
	ao_option * options;
	ao_device * device;
} AoData;

static AoData * newAoData() {
	AoData * ret = malloc(sizeof(AoData));
	ret->device = NULL;
	ret->options = NULL;

	return ret;
}

static void audioOutputAo_error() {
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

static void audioOutputAo_initDriver(AudioOutput * audioOutput) {
	ao_info * ai;
	char * dup;
	char * stk1;
	char * stk2;
	char * n1;
	char * key;
	char * value;
	char * test;
	AoData * ad  = newAoData();

	audioOutput->data = ad;

	ad->writeSize = strtol((getConf())[CONF_AUDIO_WRITE_SIZE],&test,10);
	if (*test!='\0') {
		ERROR("\"%s\" is not a valid write size",
			(getConf())[CONF_AUDIO_WRITE_SIZE]);
		exit(EXIT_FAILURE);
	}

	if(driverInitCount == 0) {
		ao_initialize();
	}
	driverInitCount++;
	
	if(strcmp(AUDIO_AO_DRIVER_DEFAULT,(getConf())[CONF_AO_DRIVER])==0) {
		ad->driverId = ao_default_driver_id();
	}
	else if((ad->driverId = 
		ao_driver_id((getConf())[CONF_AO_DRIVER]))<0) {
		ERROR("\"%s\" is not a valid ao driver\n",
			(getConf())[CONF_AO_DRIVER]);
		exit(EXIT_FAILURE);
	}
	
	if((ai = ao_driver_info(ad->driverId))==NULL) {
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
			ao_append_option(&ad->options,key,value);
			n1 = strtok_r(NULL,";",&stk1);
		}
	}
	free(dup);
}

static void freeAoData(AoData * ad) {
	ao_free_options(ad->options);
	free(ad);
}

static void audioOutputAo_finishDriver(AudioOutput * audioOutput) {
	AoData * ad = (AoData *)audioOutput->data;
	freeAoData(ad);

	driverInitCount--;

	if(driverInitCount == 0) ao_shutdown();
}

static void audioOutputAo_closeDevice(AudioOutput * audioOutput) {
	AoData * ad = (AoData *) audioOutput->data;

	if(ad->device) {
		blockSignals();
		ao_close(ad->device);
		audioOutput->open = 0;
		ad->device = NULL;
		unblockSignals();
	}
}

static int audioOutputAo_openDevice(AudioOutput * audioOutput,
		AudioFormat * audioFormat) 
{
	ao_sample_format format;
	AoData * ad = (AoData *)audioOutput->data;

	if(ad->device) {
		audioOutputAo_closeDevice(audioOutput);
	}

	format.bits = audioFormat->bits;
	format.rate = audioFormat->sampleRate;
	format.byte_format = AO_FMT_NATIVE;
	format.channels = audioFormat->channels;

	blockSignals();
	ad->device = ao_open_live(ad->driverId, &format, ad->options);
	unblockSignals();

	if(ad->device==NULL) return -1;

	audioOutput->open = 1;

	return 0;
}


static int audioOutputAo_play(AudioOutput * audioOutput, char * playChunk, 
		int size) 
{
	int send;
	AoData * ad = (AoData *)audioOutput->data;

	if(ad->device==NULL) {
		ERROR("trying to play w/o the ao device being open!\n");
		return -1;
	}
	
	while(size>0) {
		send = ad->writeSize > size ? size : ad->writeSize;
		
		if(ao_play(ad->device, playChunk, send)==0) {
			audioOutputAo_error();
			ERROR("closing audio device due to write error\n");
			audioOutputAo_closeDevice(audioOutput);
			return -1;
		}

		playChunk+=send;
		size-=send;
	}

	return 0;
}

AudioOutputPlugin aoPlugin = 
{
	"ao",
	audioOutputAo_initDriver,
	audioOutputAo_finishDriver,
	audioOutputAo_openDevice,
	audioOutputAo_play,
	audioOutputAo_closeDevice
};
