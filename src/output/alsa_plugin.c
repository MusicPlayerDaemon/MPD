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
#include "../mixer_api.h"

#include <glib.h>
#include <alsa/asoundlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "alsa"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

static const char default_device[] = "default";

enum {
	MPD_ALSA_BUFFER_TIME_US = 500000,
	MPD_ALSA_PERIOD_TIME_US = 125000,
};

#define MPD_ALSA_RETRY_NR 5

typedef snd_pcm_sframes_t alsa_writei_t(snd_pcm_t * pcm, const void *buffer,
					snd_pcm_uframes_t size);

typedef struct _AlsaData {
	char *device;

	/** the mode flags passed to snd_pcm_open */
	int mode;

	snd_pcm_t *pcmHandle;
	alsa_writei_t *writei;
	unsigned int buffer_time;
	unsigned int period_time;
	int sampleSize;
	int useMmap;

	struct mixer mixer;

} AlsaData;

static const char *
alsa_device(const AlsaData *ad)
{
	return ad->device != NULL ? ad->device : default_device;
}

static AlsaData *newAlsaData(void)
{
	AlsaData *ret = g_new(AlsaData, 1);

	ret->device = NULL;
	ret->mode = 0;
	ret->pcmHandle = NULL;
	ret->writei = snd_pcm_writei;
	ret->useMmap = 0;
	ret->buffer_time = MPD_ALSA_BUFFER_TIME_US;
	ret->period_time = MPD_ALSA_PERIOD_TIME_US;

	//use alsa mixer by default
	mixer_init(&ret->mixer, &alsa_mixer);

	return ret;
}

static void freeAlsaData(AlsaData * ad)
{
	g_free(ad->device);
	mixer_finish(&ad->mixer);
	free(ad);
}

static void
alsa_configure(AlsaData *ad, struct config_param *param)
{
	ad->device = config_dup_block_string(param, "device", NULL);

	ad->useMmap = config_get_block_bool(param, "use_mmap", false);

	ad->buffer_time = config_get_block_unsigned(param, "buffer_time",
			MPD_ALSA_BUFFER_TIME_US);
	ad->period_time = config_get_block_unsigned(param, "period_time",
			MPD_ALSA_PERIOD_TIME_US);

#ifdef SND_PCM_NO_AUTO_RESAMPLE
	if (!config_get_block_bool(param, "auto_resample", true))
		ad->mode |= SND_PCM_NO_AUTO_RESAMPLE;
#endif

#ifdef SND_PCM_NO_AUTO_CHANNELS
	if (!config_get_block_bool(param, "auto_channels", true))
		ad->mode |= SND_PCM_NO_AUTO_CHANNELS;
#endif

#ifdef SND_PCM_NO_AUTO_FORMAT
	if (!config_get_block_bool(param, "auto_format", true))
		ad->mode |= SND_PCM_NO_AUTO_FORMAT;
#endif
}

static void *
alsa_initDriver(G_GNUC_UNUSED struct audio_output *ao,
		G_GNUC_UNUSED const struct audio_format *audio_format,
		struct config_param *param)
{
	/* no need for pthread_once thread-safety when reading config */
	static int free_global_registered;
	AlsaData *ad = newAlsaData();

	if (!free_global_registered) {
		atexit((void(*)(void))snd_config_update_free_global);
		free_global_registered = 1;
	}

	if (param) {
		alsa_configure(ad, param);
		mixer_configure(&ad->mixer, param);
	}

	return ad;
}

static void alsa_finishDriver(void *data)
{
	AlsaData *ad = data;

	freeAlsaData(ad);
}

static bool alsa_testDefault(void)
{
	snd_pcm_t *handle;

	int ret = snd_pcm_open(&handle, default_device,
	                       SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (ret) {
		g_message("Error opening default ALSA device: %s\n",
			  snd_strerror(-ret));
		return false;
	} else
		snd_pcm_close(handle);

	return true;
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

static bool alsa_openDevice(void *data, struct audio_format *audioFormat)
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

	mixer_open(&ad->mixer);

	if ((bitformat = get_bitformat(audioFormat)) == SND_PCM_FORMAT_UNKNOWN)
		g_warning("ALSA device \"%s\" doesn't support %u bit audio\n",
			  alsa_device(ad), audioFormat->bits);

	err = snd_pcm_open(&ad->pcmHandle, alsa_device(ad),
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
			g_warning("Cannot set mmap'ed mode on ALSA device \"%s\":  %s\n",
				  alsa_device(ad), snd_strerror(-err));
			g_warning("Falling back to direct write mode\n");
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
		/* fall back to 16 bit, let pcm_convert.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcmHandle, hwparams,
						   SND_PCM_FORMAT_S16);
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to 16 bit\n",
				alsa_device(ad), audioFormat->bits);
			audioFormat->bits = 16;
		}
	}

	if (err < 0) {
		g_warning("ALSA device \"%s\" does not support %u bit audio: %s\n",
			  alsa_device(ad), audioFormat->bits, snd_strerror(-err));
		goto fail;
	}

	err = snd_pcm_hw_params_set_channels_near(ad->pcmHandle, hwparams,
						  &channels);
	if (err < 0) {
		g_warning("ALSA device \"%s\" does not support %i channels: %s\n",
			  alsa_device(ad), (int)audioFormat->channels,
		      snd_strerror(-err));
		goto fail;
	}
	audioFormat->channels = (int8_t)channels;

	err = snd_pcm_hw_params_set_rate_near(ad->pcmHandle, hwparams,
					      &sample_rate, NULL);
	if (err < 0 || sample_rate == 0) {
		g_warning("ALSA device \"%s\" does not support %u Hz audio\n",
			  alsa_device(ad), audioFormat->sample_rate);
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
		g_debug("ALSA period_time set to %d\n", period_time);

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

	g_debug("ALSA device \"%s\" will be playing %i bit, %u channel audio at %u Hz\n",
		alsa_device(ad), audioFormat->bits, channels, sample_rate);

	return true;

error:
	if (cmd) {
		g_warning("Error opening ALSA device \"%s\" (%s): %s\n",
			  alsa_device(ad), cmd, snd_strerror(-err));
	} else {
		g_warning("Error opening ALSA device \"%s\": %s\n",
			  alsa_device(ad), snd_strerror(-err));
	}
fail:
	if (ad->pcmHandle)
		snd_pcm_close(ad->pcmHandle);
	ad->pcmHandle = NULL;
	return false;
}

static int alsa_errorRecovery(AlsaData * ad, int err)
{
	if (err == -EPIPE) {
		g_debug("Underrun on ALSA device \"%s\"\n", alsa_device(ad));
	} else if (err == -ESTRPIPE) {
		g_debug("ALSA device \"%s\" was suspended\n", alsa_device(ad));
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
	mixer_close(&ad->mixer);
}

static bool
alsa_playAudio(void *data, const char *playChunk, size_t size)
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
				g_warning("closing ALSA device \"%s\" due to write "
					  "error: %s\n",
					  alsa_device(ad), snd_strerror(-errno));
				return false;
			}
			continue;
		}

		playChunk += ret * ad->sampleSize;
		size -= ret;
	}

	return true;
}

static bool
alsa_control(void *data, int cmd, void *arg)
{
	AlsaData *ad = data;
	return mixer_control(&ad->mixer, cmd, arg);
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
	.control = alsa_control
};
