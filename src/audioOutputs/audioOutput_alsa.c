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

#include "../output_api.h"

#ifdef HAVE_ALSA

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

static const char default_device[] = "default";

#define MPD_ALSA_RETRY_NR 5

#include "../utils.h"
#include "../log.h"

#include <alsa/asoundlib.h>

typedef snd_pcm_sframes_t alsa_writei_t(snd_pcm_t * pcm, const void *buffer,
					snd_pcm_uframes_t size);

typedef struct _AlsaData {
	const char *device;

	/** the mode flags passed to snd_pcm_open */
	int mode;

	snd_pcm_t *pcmHandle;
	alsa_writei_t *writei;
	unsigned int buffer_time;
	unsigned int period_time;
	int sampleSize;
	int useMmap;
} AlsaData;

static AlsaData *newAlsaData(void)
{
	AlsaData *ret = xmalloc(sizeof(AlsaData));

	ret->device = default_device;
	ret->mode = 0;
	ret->pcmHandle = NULL;
	ret->writei = snd_pcm_writei;
	ret->useMmap = 0;
	ret->buffer_time = 0;
	ret->period_time = 0;

	return ret;
}

static void freeAlsaData(AlsaData * ad)
{
	if (ad->device && ad->device != default_device)
		xfree(ad->device);
	free(ad);
}

static void
alsa_configure(AlsaData *ad, ConfigParam *param)
{
	BlockParam *bp;

	if ((bp = getBlockParam(param, "device")))
		ad->device = xstrdup(bp->value);
	ad->useMmap = getBoolBlockParam(param, "use_mmap", 1);
	if (ad->useMmap == CONF_BOOL_UNSET)
		ad->useMmap = 0;
	if ((bp = getBlockParam(param, "buffer_time")))
		ad->buffer_time = atoi(bp->value);
	if ((bp = getBlockParam(param, "period_time")))
		ad->period_time = atoi(bp->value);

	if (!getBoolBlockParam(param, "auto_resample", true))
		ad->mode |= SND_PCM_NO_AUTO_RESAMPLE;

	if (!getBoolBlockParam(param, "auto_channels", true))
		ad->mode |= SND_PCM_NO_AUTO_CHANNELS;

	if (!getBoolBlockParam(param, "auto_format", true))
		ad->mode |= SND_PCM_NO_AUTO_FORMAT;
}

static void *alsa_initDriver(mpd_unused struct audio_output *ao,
			     mpd_unused const struct audio_format *audio_format,
			     ConfigParam * param)
{
	/* no need for pthread_once thread-safety when reading config */
	static int free_global_registered;
	AlsaData *ad = newAlsaData();

	if (!free_global_registered) {
		atexit((void(*)(void))snd_config_update_free_global);
		free_global_registered = 1;
	}

	if (param)
		alsa_configure(ad, param);

	return ad;
}

static void alsa_finishDriver(void *data)
{
	AlsaData *ad = data;

	freeAlsaData(ad);
}

static int alsa_testDefault(void)
{
	snd_pcm_t *handle;

	int ret = snd_pcm_open(&handle, default_device,
	                       SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (ret) {
		WARNING("Error opening default ALSA device: %s\n",
			snd_strerror(-ret));
		return -1;
	} else
		snd_pcm_close(handle);

	return 0;
}

static snd_pcm_format_t get_bitformat(const struct audio_format *af)
{
	switch (af->bits) {
	case 8: return SND_PCM_FORMAT_S8;
	case 16: return SND_PCM_FORMAT_S16;
	case 24: return SND_PCM_FORMAT_S24;
	case 32: return SND_PCM_FORMAT_S32;
	}
	return SND_PCM_FORMAT_UNKNOWN;
}

static int alsa_openDevice(void *data, struct audio_format *audioFormat)
{
	AlsaData *ad = data;
	snd_pcm_format_t bitformat;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	unsigned int sample_rate = audioFormat->sample_rate;
	unsigned int channels = audioFormat->channels;
	snd_pcm_uframes_t alsa_buffer_size;
	snd_pcm_uframes_t alsa_period_size;
	int err;
	const char *cmd = NULL;
	int retry = MPD_ALSA_RETRY_NR;
	unsigned int period_time, period_time_ro;
	unsigned int buffer_time;

	if ((bitformat = get_bitformat(audioFormat)) == SND_PCM_FORMAT_UNKNOWN)
		ERROR("ALSA device \"%s\" doesn't support %u bit audio\n",
		      ad->device, audioFormat->bits);

	err = snd_pcm_open(&ad->pcmHandle, ad->device,
			   SND_PCM_STREAM_PLAYBACK, ad->mode);
	if (err < 0) {
		ad->pcmHandle = NULL;
		goto error;
	}

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
			ERROR("Cannot set mmap'ed mode on ALSA device \"%s\": "
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
	if (err == -EINVAL && audioFormat->bits != 16) {
		/* fall back to 16 bit, let pcm_utils.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcmHandle, hwparams,
						   SND_PCM_FORMAT_S16);
		if (err == 0) {
			DEBUG("ALSA device \"%s\": converting %u bit to 16 bit\n",
			      ad->device, audioFormat->bits);
			audioFormat->bits = 16;
		}
	}

	if (err < 0) {
		ERROR("ALSA device \"%s\" does not support %u bit audio: "
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
	audioFormat->channels = (int8_t)channels;

	err = snd_pcm_hw_params_set_rate_near(ad->pcmHandle, hwparams,
					      &sample_rate, NULL);
	if (err < 0 || sample_rate == 0) {
		ERROR("ALSA device \"%s\" does not support %u Hz audio\n",
		      ad->device, audioFormat->sample_rate);
		goto fail;
	}
	audioFormat->sample_rate = sample_rate;

	if (ad->buffer_time > 0) {
		buffer_time = ad->buffer_time;
		cmd = "snd_pcm_hw_params_set_buffer_time_near";
		err = snd_pcm_hw_params_set_buffer_time_near(ad->pcmHandle, hwparams,
							     &buffer_time, NULL);
		if (err < 0)
			goto error;
	}

	if (period_time_ro > 0) {
		period_time = period_time_ro;
		cmd = "snd_pcm_hw_params_set_period_time_near";
		err = snd_pcm_hw_params_set_period_time_near(ad->pcmHandle, hwparams,
							     &period_time, NULL);
		if (err < 0)
			goto error;
	}

	cmd = "snd_pcm_hw_params";
	err = snd_pcm_hw_params(ad->pcmHandle, hwparams);
	if (err == -EPIPE && --retry > 0 && period_time_ro > 0) {
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

	cmd = "snd_pcm_sw_params";
	err = snd_pcm_sw_params(ad->pcmHandle, swparams);
	if (err < 0)
		goto error;

	ad->sampleSize = audio_format_frame_size(audioFormat);

	DEBUG("ALSA device \"%s\" will be playing %i bit, %u channel audio at "
	      "%u Hz\n", ad->device, audioFormat->bits,
	      channels, sample_rate);

	return 0;

error:
	if (cmd) {
		ERROR("Error opening ALSA device \"%s\" (%s): %s\n",
		      ad->device, cmd, snd_strerror(-err));
	} else {
		ERROR("Error opening ALSA device \"%s\": %s\n", ad->device,
		      snd_strerror(-err));
	}
fail:
	if (ad->pcmHandle)
		snd_pcm_close(ad->pcmHandle);
	ad->pcmHandle = NULL;
	return -1;
}

static int alsa_errorRecovery(AlsaData * ad, int err)
{
	if (err == -EPIPE) {
		DEBUG("Underrun on ALSA device \"%s\"\n", ad->device);
	} else if (err == -ESTRPIPE) {
		DEBUG("ALSA device \"%s\" was suspended\n", ad->device);
	}

	switch (snd_pcm_state(ad->pcmHandle)) {
	case SND_PCM_STATE_PAUSED:
		err = snd_pcm_pause(ad->pcmHandle, /* disable */ 0);
		break;
	case SND_PCM_STATE_SUSPENDED:
		err = snd_pcm_resume(ad->pcmHandle);
		if (err == -EAGAIN)
			return 0;
		/* fall-through to snd_pcm_prepare: */
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

static void alsa_dropBufferedAudio(void *data)
{
	AlsaData *ad = data;

	alsa_errorRecovery(ad, snd_pcm_drop(ad->pcmHandle));
}

static void alsa_closeDevice(void *data)
{
	AlsaData *ad = data;

	if (ad->pcmHandle) {
		if (snd_pcm_state(ad->pcmHandle) == SND_PCM_STATE_RUNNING) {
			snd_pcm_drain(ad->pcmHandle);
		}
		snd_pcm_close(ad->pcmHandle);
		ad->pcmHandle = NULL;
	}
}

static int alsa_playAudio(void *data, const char *playChunk, size_t size)
{
	AlsaData *ad = data;
	int ret;

	size /= ad->sampleSize;

	while (size > 0) {
		ret = ad->writei(ad->pcmHandle, playChunk, size);

		if (ret == -EAGAIN || ret == -EINTR)
			continue;

		if (ret < 0) {
			if (alsa_errorRecovery(ad, ret) < 0) {
				ERROR("closing ALSA device \"%s\" due to write "
				      "error: %s\n", ad->device,
				      snd_strerror(-errno));
				alsa_closeDevice(ad);
				return -1;
			}
			continue;
		}

		playChunk += ret * ad->sampleSize;
		size -= ret;
	}

	return 0;
}

const struct audio_output_plugin alsaPlugin = {
	.name = "alsa",
	.test_default_device = alsa_testDefault,
	.init = alsa_initDriver,
	.finish = alsa_finishDriver,
	.open = alsa_openDevice,
	.play = alsa_playAudio,
	.cancel = alsa_dropBufferedAudio,
	.close = alsa_closeDevice,
};

#else /* HAVE ALSA */

DISABLED_AUDIO_OUTPUT_PLUGIN(alsaPlugin)
#endif /* HAVE_ALSA */
