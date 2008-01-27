/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#define MPD_ALSA_BUFFER_TIME_US 500000
/* the default period time of xmms is 50 ms, so let's use that as well.
 * a user can tweak this parameter via the "period_time" config parameter.
 */
#define MPD_ALSA_PERIOD_TIME_US 50000
#define MPD_ALSA_RETRY_NR 5

#include "../conf.h"
#include "../log.h"

#include <string.h>

#include <alsa/asoundlib.h>

typedef snd_pcm_sframes_t alsa_writei_t(snd_pcm_t * pcm, const void *buffer,
					snd_pcm_uframes_t size);

typedef struct _AlsaData {
	char *device;
	snd_pcm_t *pcmHandle;
	alsa_writei_t *writei;
	unsigned int buffer_time;
	unsigned int period_time;
	int sampleSize;
	int useMmap;
	int canPause;
	int canResume;
} AlsaData;

static AlsaData *newAlsaData(void)
{
	AlsaData *ret = xmalloc(sizeof(AlsaData));

	ret->device = NULL;
	ret->pcmHandle = NULL;
	ret->writei = snd_pcm_writei;
	ret->useMmap = 0;
	ret->buffer_time = MPD_ALSA_BUFFER_TIME_US;
	ret->period_time = MPD_ALSA_PERIOD_TIME_US;

	return ret;
}

static void freeAlsaData(AlsaData * ad)
{
	if (ad->device)
		free(ad->device);

	free(ad);
}

static int alsa_initDriver(AudioOutput * audioOutput, ConfigParam * param)
{
	AlsaData *ad = newAlsaData();

	if (param) {
		BlockParam *bp = getBlockParam(param, "device");
		ad->device = bp ? xstrdup(bp->value) : xstrdup("default");

		if ((bp = getBlockParam(param, "use_mmap")) &&
		    !strcasecmp(bp->value, "yes"))
			ad->useMmap = 1;
		if ((bp = getBlockParam(param, "buffer_time")))
			ad->buffer_time = atoi(bp->value);
		if ((bp = getBlockParam(param, "period_time")))
			ad->period_time = atoi(bp->value);
	} else
		ad->device = xstrdup("default");
	audioOutput->data = ad;

	return 0;
}

static void alsa_finishDriver(AudioOutput * audioOutput)
{
	AlsaData *ad = audioOutput->data;

	freeAlsaData(ad);
}

static int alsa_testDefault(void)
{
	snd_pcm_t *handle;

	int ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK,
			       SND_PCM_NONBLOCK);
	snd_config_update_free_global();

	if (ret) {
		WARNING("Error opening default alsa device: %s\n",
			snd_strerror(-ret));
		return -1;
	} else
		snd_pcm_close(handle);

	return 0;
}

static int alsa_openDevice(AudioOutput * audioOutput)
{
	AlsaData *ad = audioOutput->data;
	AudioFormat *audioFormat = &audioOutput->outAudioFormat;
	snd_pcm_format_t bitformat;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	unsigned int sampleRate = audioFormat->sampleRate;
	unsigned int channels = audioFormat->channels;
	snd_pcm_uframes_t alsa_buffer_size;
	snd_pcm_uframes_t alsa_period_size;
	int err;
	const char *cmd = NULL;
	int retry = MPD_ALSA_RETRY_NR;
	unsigned int period_time, period_time_ro;
	unsigned int buffer_time;

	switch (audioFormat->bits) {
	case 8:
		bitformat = SND_PCM_FORMAT_S8;
		break;
	case 16:
		bitformat = SND_PCM_FORMAT_S16;
		break;
	case 24:
		bitformat = SND_PCM_FORMAT_S24;
		break;
	case 32:
		bitformat = SND_PCM_FORMAT_S32;
		break;
	default:
		ERROR("ALSA device \"%s\" doesn't support %i bit audio\n",
		      ad->device, audioFormat->bits);
		return -1;
	}

	err = snd_pcm_open(&ad->pcmHandle, ad->device,
			   SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	snd_config_update_free_global();
	if (err < 0) {
		ad->pcmHandle = NULL;
		goto error;
	}

	cmd = "snd_pcm_nonblock";
	err = snd_pcm_nonblock(ad->pcmHandle, 0);
	if (err < 0)
		goto error;

	period_time_ro = period_time = ad->period_time;
configure_hw:
	/* configure HW params */
	snd_pcm_hw_params_alloca(&hwparams);

	cmd = "snd_pcm_hw_params_any";
	err = snd_pcm_hw_params_any(ad->pcmHandle, hwparams);
	if (err < 0)
		goto error;

	if (ad->useMmap) {
		err = snd_pcm_hw_params_set_access(ad->pcmHandle, hwparams,
						   SND_PCM_ACCESS_MMAP_INTERLEAVED);
		if (err < 0) {
			ERROR("Cannot set mmap'ed mode on alsa device \"%s\": "
			      " %s\n", ad->device, snd_strerror(-err));
			ERROR("Falling back to direct write mode\n");
			ad->useMmap = 0;
		} else
			ad->writei = snd_pcm_mmap_writei;
	}

	if (!ad->useMmap) {
		cmd = "snd_pcm_hw_params_set_access";
		err = snd_pcm_hw_params_set_access(ad->pcmHandle, hwparams,
						   SND_PCM_ACCESS_RW_INTERLEAVED);
		if (err < 0)
			goto error;
		ad->writei = snd_pcm_writei;
	}

	err = snd_pcm_hw_params_set_format(ad->pcmHandle, hwparams, bitformat);
	if (err < 0) {
		ERROR("ALSA device \"%s\" does not support %i bit audio: "
		      "%s\n", ad->device, audioFormat->bits, snd_strerror(-err));
		goto fail;
	}

	err = snd_pcm_hw_params_set_channels_near(ad->pcmHandle, hwparams,
						  &channels);
	if (err < 0) {
		ERROR("ALSA device \"%s\" does not support %i channels: "
		      "%s\n", ad->device, (int)audioFormat->channels,
		      snd_strerror(-err));
		goto fail;
	}
	audioFormat->channels = channels;

	err = snd_pcm_hw_params_set_rate_near(ad->pcmHandle, hwparams,
					      &sampleRate, NULL);
	if (err < 0 || sampleRate == 0) {
		ERROR("ALSA device \"%s\" does not support %i Hz audio\n",
		      ad->device, (int)audioFormat->sampleRate);
		goto fail;
	}
	audioFormat->sampleRate = sampleRate;

	buffer_time = ad->buffer_time;
	cmd = "snd_pcm_hw_params_set_buffer_time_near";
	err = snd_pcm_hw_params_set_buffer_time_near(ad->pcmHandle, hwparams,
						     &buffer_time, NULL);
	if (err < 0)
		goto error;

	period_time = period_time_ro;
	cmd = "snd_pcm_hw_params_set_period_time_near";
	err = snd_pcm_hw_params_set_period_time_near(ad->pcmHandle, hwparams,
						     &period_time, NULL);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_hw_params";
	err = snd_pcm_hw_params(ad->pcmHandle, hwparams);
	if (err == -EPIPE && --retry > 0) {
		period_time_ro = period_time_ro >> 1;
		goto configure_hw;
	} else if (err < 0)
		goto error;
	if (retry != MPD_ALSA_RETRY_NR)
		DEBUG("ALSA period_time set to %d\n", period_time);

	cmd = "snd_pcm_hw_params_get_buffer_size";
	err = snd_pcm_hw_params_get_buffer_size(hwparams, &alsa_buffer_size);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_hw_params_get_period_size";
	err = snd_pcm_hw_params_get_period_size(hwparams, &alsa_period_size,
						NULL);
	if (err < 0)
		goto error;

	ad->canPause = snd_pcm_hw_params_can_pause(hwparams);
	ad->canResume = snd_pcm_hw_params_can_resume(hwparams);

	/* configure SW params */
	snd_pcm_sw_params_alloca(&swparams);

	cmd = "snd_pcm_sw_params_current";
	err = snd_pcm_sw_params_current(ad->pcmHandle, swparams);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params_set_start_threshold";
	err = snd_pcm_sw_params_set_start_threshold(ad->pcmHandle, swparams,
						    alsa_buffer_size -
						    alsa_period_size);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params_set_avail_min";
	err = snd_pcm_sw_params_set_avail_min(ad->pcmHandle, swparams,
					      alsa_period_size);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params_set_xfer_align";
	err = snd_pcm_sw_params_set_xfer_align(ad->pcmHandle, swparams, 1);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params";
	err = snd_pcm_sw_params(ad->pcmHandle, swparams);
	if (err < 0)
		goto error;

	ad->sampleSize = (audioFormat->bits / 8) * audioFormat->channels;

	audioOutput->open = 1;

	DEBUG("alsa device \"%s\" will be playing %i bit, %i channel audio at "
	      "%i Hz\n", ad->device, (int)audioFormat->bits,
	      channels, sampleRate);

	return 0;

error:
	if (cmd) {
		ERROR("Error opening alsa device \"%s\" (%s): %s\n",
		      ad->device, cmd, snd_strerror(-err));
	} else {
		ERROR("Error opening alsa device \"%s\": %s\n", ad->device,
		      snd_strerror(-err));
	}
fail:
	if (ad->pcmHandle)
		snd_pcm_close(ad->pcmHandle);
	ad->pcmHandle = NULL;
	audioOutput->open = 0;
	return -1;
}

static int alsa_errorRecovery(AlsaData * ad, int err)
{
	if (err == -EPIPE) {
		DEBUG("Underrun on alsa device \"%s\"\n", ad->device);
	} else if (err == -ESTRPIPE) {
		DEBUG("alsa device \"%s\" was suspended\n", ad->device);
	}

	switch (snd_pcm_state(ad->pcmHandle)) {
	case SND_PCM_STATE_PAUSED:
		err = snd_pcm_pause(ad->pcmHandle, /* disable */ 0);
		break;
	case SND_PCM_STATE_SUSPENDED:
		err = ad->canResume ?
		    snd_pcm_resume(ad->pcmHandle) :
		    snd_pcm_prepare(ad->pcmHandle);
		break;
	case SND_PCM_STATE_SETUP:
	case SND_PCM_STATE_XRUN:
		err = snd_pcm_prepare(ad->pcmHandle);
		break;
	case SND_PCM_STATE_DISCONNECTED:
		/* so alsa_closeDevice won't try to drain: */
		snd_pcm_close(ad->pcmHandle);
		ad->pcmHandle = NULL;
		break;
        /* this is no error, so just keep running */
    case SND_PCM_STATE_RUNNING:
        err = 0;
        break;
	default:
		/* unknown state, do nothing */
		break;
	}

	return err;
}

static void alsa_dropBufferedAudio(AudioOutput * audioOutput)
{
	AlsaData *ad = audioOutput->data;

	alsa_errorRecovery(ad, snd_pcm_drop(ad->pcmHandle));
}

static void alsa_closeDevice(AudioOutput * audioOutput)
{
	AlsaData *ad = audioOutput->data;

	if (ad->pcmHandle) {
                if (snd_pcm_state(ad->pcmHandle) == SND_PCM_STATE_RUNNING) {
                        snd_pcm_drain(ad->pcmHandle);
                }
		snd_pcm_close(ad->pcmHandle);
		ad->pcmHandle = NULL;
	}

	audioOutput->open = 0;
}

static int alsa_playAudio(AudioOutput * audioOutput, char *playChunk, int size)
{
	AlsaData *ad = audioOutput->data;
	int ret;

	size /= ad->sampleSize;

	while (size > 0) {
		ret = ad->writei(ad->pcmHandle, playChunk, size);

		if (ret == -EAGAIN || ret == -EINTR)
			continue;

		if (ret < 0) {
			if (alsa_errorRecovery(ad, ret) < 0) {
				ERROR("closing alsa device \"%s\" due to write "
				      "error: %s\n", ad->device,
				      snd_strerror(-errno));
				alsa_closeDevice(audioOutput);
				return -1;
			}
			continue;
		}

		playChunk += ret * ad->sampleSize;
		size -= ret;
	}

	return 0;
}

AudioOutputPlugin alsaPlugin = {
	"alsa",
	alsa_testDefault,
	alsa_initDriver,
	alsa_finishDriver,
	alsa_openDevice,
	alsa_playAudio,
	alsa_dropBufferedAudio,
	alsa_closeDevice,
	NULL,	/* sendMetadataFunc */
};

#else /* HAVE ALSA */

DISABLED_AUDIO_OUTPUT_PLUGIN(alsaPlugin)
#endif /* HAVE_ALSA */
