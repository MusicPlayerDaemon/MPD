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

#include "../audioOutput.h"

#include <stdlib.h>

#ifdef HAVE_ALSA

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#define MPD_ALSA_BUFFER_TIME 500000
#define MPD_ALSA_PERIOD_TIME 50000

#include "../conf.h"
#include "../log.h"
#include "../sig_handlers.h"

#include <string.h>
#include <assert.h>
#include <signal.h>

#include <alsa/asoundlib.h>

typedef snd_pcm_sframes_t alsa_writei_t(snd_pcm_t *pcm, const void *buffer,
                                                snd_pcm_uframes_t size);

typedef struct _AlsaData {
	char * device;
	snd_pcm_t * pcm_handle;
	int mmap;
	alsa_writei_t * writei;
	int sampleSize;
} AlsaData;

static AlsaData * newAlsaData() {
	AlsaData * ret = malloc(sizeof(AlsaData));

	ret->device = NULL;
	ret->pcm_handle = NULL;
	ret->writei = snd_pcm_writei;
	ret->mmap = 0;

	return ret;
}

static void freeAlsaData(AlsaData * ad) {
	if(ad->device) free(ad->device);

	free(ad);
}

static int alsa_initDriver(AudioOutput * audioOutput, ConfigParam * param) {
	BlockParam * bp = getBlockParam(param, "device");
	AlsaData * ad = newAlsaData();
	
	audioOutput->data = ad;

	ad->device = bp ? strdup(bp->value) : strdup("default");

	return 0;
}

static void alsa_finishDriver(AudioOutput * audioOutput) {
	AlsaData * ad = audioOutput->data;

	freeAlsaData(ad);
}

static int alsa_openDevice(AudioOutput * audioOutput) 
{
	AlsaData * ad = audioOutput->data;
	AudioFormat * audioFormat = &audioOutput->outAudioFormat;
	snd_pcm_format_t bitformat;
	snd_pcm_hw_params_t * hwparams;
	snd_pcm_sw_params_t * swparams;
	unsigned int sampleRate = audioFormat->sampleRate;
	snd_pcm_uframes_t alsa_buffer_size;
	snd_pcm_uframes_t alsa_period_size;
	unsigned int alsa_buffer_time = MPD_ALSA_BUFFER_TIME;
	unsigned int alsa_period_time = MPD_ALSA_PERIOD_TIME;
	int err;

	switch(audioFormat->bits) {
	case 8:
		bitformat = SND_PCM_FORMAT_S8;
		break;
	case 16:
		bitformat = SND_PCM_FORMAT_S16;
		break;
	case 24:
		bitformat = SND_PCM_FORMAT_S16;
		break;
	case 32:
		bitformat = SND_PCM_FORMAT_S16;
		break;
	default:
		ERROR("Alsa device \"%s\" doesn't support %i bit audio\n",
				ad->device, audioFormat->bits);
		return -1;
	}

	err = snd_pcm_open(&ad->pcm_handle, ad->device, 
			SND_PCM_STREAM_PLAYBACK, 0);
	if(err < 0) {
		ad->pcm_handle = NULL;
		goto error;
	}

	/*err = snd_pcm_nonblock(ad->pcm_handle, 0);
	if(err < 0) goto error;*/

	/* configure HW params */
	snd_pcm_hw_params_alloca(&hwparams);

	err = snd_pcm_hw_params_any(ad->pcm_handle, hwparams);
	if(err < 0) goto error;

	if(ad->mmap) {
		err = snd_pcm_hw_params_set_access(ad->pcm_handle, hwparams,
				SND_PCM_ACCESS_MMAP_INTERLEAVED);
		if(err < 0) {
			ERROR("Cannot set mmap'ed mode on alsa device \"%s\": "
					" %s\n", ad->device, 
					snd_strerror(-err));
			ERROR("Falling back to direct write mode\n");
			ad->mmap = 0;
		}
		else ad->writei = snd_pcm_mmap_writei;
	}

	if(!ad->mmap) {
		err = snd_pcm_hw_params_set_access(ad->pcm_handle, hwparams,
				SND_PCM_ACCESS_RW_INTERLEAVED);
		if(err < 0) goto error;
		ad->writei = snd_pcm_writei;
	}

	err = snd_pcm_hw_params_set_format(ad->pcm_handle, hwparams, bitformat);
	if(err < 0) {
		ERROR("Alsa device \"%s\" does not support %i bit audio: "
				"%s\n", ad->device, (int)bitformat, 
				snd_strerror(-err));
		goto fail;
	}

	err = snd_pcm_hw_params_set_channels(ad->pcm_handle, hwparams, 
			audioFormat->channels);
	if(err < 0) {
		ERROR("Alsa device \"%s\" does not support %i channels: "
				"%s\n", ad->device, (int)audioFormat->channels, 
				snd_strerror(-err));
		goto fail;
	}

	err = snd_pcm_hw_params_set_rate_near(ad->pcm_handle, hwparams, 
			&sampleRate, 0);
	if(err < 0 || sampleRate == 0) {
		ERROR("Alsa device \"%s\" does not support %i Hz audio\n",
				ad->device, (int)audioFormat->sampleRate);
		goto fail;
	}

	err = snd_pcm_hw_params_set_buffer_time_near(ad->pcm_handle, hwparams,
			&alsa_buffer_time, 0);
	if(err < 0) goto error;
	
	err = snd_pcm_hw_params_set_period_time_near(ad->pcm_handle, hwparams,
			&alsa_period_time, 0);
	if(err < 0) goto error;

	err = snd_pcm_hw_params(ad->pcm_handle, hwparams);
	if(err < 0) goto error;

	err = snd_pcm_hw_params_get_buffer_size(hwparams, &alsa_buffer_size);
	if(err < 0) goto error;

	err = snd_pcm_hw_params_get_period_size(hwparams, &alsa_period_size, 0);
	if(err < 0) goto error;

	/* configure SW params */
	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(ad->pcm_handle, swparams);

	err = snd_pcm_sw_params_set_start_threshold(ad->pcm_handle, swparams,
			alsa_buffer_size - alsa_period_size);
	if(err < 0) goto error;

	err = snd_pcm_sw_params(ad->pcm_handle, swparams);
	if(err < 0) goto error;
	
	ad->sampleSize = (audioFormat->bits/8)*audioFormat->channels;

	audioOutput->open = 1;

	return 0;

error:
	ERROR("Error opening alsa device \"%s\": %s\n", ad->device, 
			snd_strerror(-err));
fail:
	if(ad->pcm_handle) snd_pcm_close(ad->pcm_handle);
	ad->pcm_handle = NULL;
	audioOutput->open = 0;
	return -1;
}

static void alsa_closeDevice(AudioOutput * audioOutput) {
	AlsaData * ad = audioOutput->data;

	if(ad->pcm_handle) {
		snd_pcm_drain(ad->pcm_handle);
		ad->pcm_handle = NULL;
	}

	audioOutput->open = 0;
}

inline static int alsa_errorRecovery(AlsaData * ad, int err) {
	if(err == -EPIPE) {
		DEBUG("Underrun on alsa device \"%s\"\n", ad->device);
		err = snd_pcm_prepare(ad->pcm_handle);
		if(err < 0) return -1;
		return 0;
	}

	return err;
}

static int alsa_playAudio(AudioOutput * audioOutput, char * playChunk, 
		int size) 
{
	AlsaData * ad = audioOutput->data;
	int ret;

	size /= ad->sampleSize;

	while (size > 0) {
		ret = ad->writei(ad->pcm_handle, playChunk, size);

		if(ret == -EAGAIN) continue;
		
		if(ret < 0 && alsa_errorRecovery(ad, ret) < 0) {
			ERROR("closing alsa device \"%s\" due to write error:"
					" %s\n", ad->device, 
					snd_strerror(-errno));
			alsa_closeDevice(audioOutput);
			return -1;
		}
		playChunk += ret * ad->sampleSize;
		size -= ret;
	}

	return 0;
}

AudioOutputPlugin alsaPlugin =
{
	"alsa",
	alsa_initDriver,
	alsa_finishDriver,
	alsa_openDevice,
	alsa_playAudio,
	alsa_closeDevice,
	NULL /* sendMetadataFunc */
};

#else /* HAVE ALSA */

AudioOutputPlugin alsaPlugin =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL /* sendMetadataFunc */
};

#endif /* HAVE_ALSA */


