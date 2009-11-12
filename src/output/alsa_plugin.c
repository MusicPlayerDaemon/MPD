/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "output_api.h"
#include "mixer_list.h"

#include <glib.h>
#include <alsa/asoundlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "alsa"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

static const char default_device[] = "default";

enum {
	MPD_ALSA_BUFFER_TIME_US = 500000,
};

#define MPD_ALSA_RETRY_NR 5

typedef snd_pcm_sframes_t alsa_writei_t(snd_pcm_t * pcm, const void *buffer,
					snd_pcm_uframes_t size);

struct alsa_data {
	/** the configured name of the ALSA device; NULL for the
	    default device */
	char *device;

	/** use memory mapped I/O? */
	bool use_mmap;

	/** libasound's buffer_time setting (in microseconds) */
	unsigned int buffer_time;

	/** libasound's period_time setting (in microseconds) */
	unsigned int period_time;

	/** the mode flags passed to snd_pcm_open */
	int mode;

	/** the libasound PCM device handle */
	snd_pcm_t *pcm;

	/**
	 * a pointer to the libasound writei() function, which is
	 * snd_pcm_writei() or snd_pcm_mmap_writei(), depending on the
	 * use_mmap configuration
	 */
	alsa_writei_t *writei;

	/** the size of one audio frame */
	size_t frame_size;

	/**
	 * The size of one period, in number of frames.
	 */
	snd_pcm_uframes_t period_frames;

	/**
	 * The number of frames written in the current period.
	 */
	snd_pcm_uframes_t period_position;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
alsa_output_quark(void)
{
	return g_quark_from_static_string("alsa_output");
}

static const char *
alsa_device(const struct alsa_data *ad)
{
	return ad->device != NULL ? ad->device : default_device;
}

static struct alsa_data *
alsa_data_new(void)
{
	struct alsa_data *ret = g_new(struct alsa_data, 1);

	ret->mode = 0;
	ret->writei = snd_pcm_writei;

	return ret;
}

static void
alsa_data_free(struct alsa_data *ad)
{
	g_free(ad->device);
	g_free(ad);
}

static void
alsa_configure(struct alsa_data *ad, const struct config_param *param)
{
	ad->device = config_dup_block_string(param, "device", NULL);

	ad->use_mmap = config_get_block_bool(param, "use_mmap", false);

	ad->buffer_time = config_get_block_unsigned(param, "buffer_time",
			MPD_ALSA_BUFFER_TIME_US);
	ad->period_time = config_get_block_unsigned(param, "period_time", 0);

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
alsa_init(G_GNUC_UNUSED const struct audio_format *audio_format,
	  const struct config_param *param,
	  G_GNUC_UNUSED GError **error)
{
	struct alsa_data *ad = alsa_data_new();

	alsa_configure(ad, param);

	return ad;
}

static void
alsa_finish(void *data)
{
	struct alsa_data *ad = data;

	alsa_data_free(ad);

	/* free libasound's config cache */
	snd_config_update_free_global();
}

static bool
alsa_test_default_device(void)
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

static snd_pcm_format_t
get_bitformat(const struct audio_format *af)
{
	switch (af->bits) {
	case 8: return SND_PCM_FORMAT_S8;
	case 16: return SND_PCM_FORMAT_S16;
	case 24: return SND_PCM_FORMAT_S24;
	case 32: return SND_PCM_FORMAT_S32;
	}
	return SND_PCM_FORMAT_UNKNOWN;
}

static snd_pcm_format_t
byteswap_bitformat(snd_pcm_format_t fmt)
{
	switch(fmt) {
	case SND_PCM_FORMAT_S16_LE: return SND_PCM_FORMAT_S16_BE;
	case SND_PCM_FORMAT_S24_LE: return SND_PCM_FORMAT_S24_BE;
	case SND_PCM_FORMAT_S32_LE: return SND_PCM_FORMAT_S32_BE;
	case SND_PCM_FORMAT_S16_BE: return SND_PCM_FORMAT_S16_LE;
	case SND_PCM_FORMAT_S24_BE: return SND_PCM_FORMAT_S24_LE;
	case SND_PCM_FORMAT_S32_BE: return SND_PCM_FORMAT_S32_LE;
	default: return SND_PCM_FORMAT_UNKNOWN;
	}
}
/**
 * Set up the snd_pcm_t object which was opened by the caller.  Set up
 * the configured settings and the audio format.
 */
static bool
alsa_setup(struct alsa_data *ad, struct audio_format *audio_format,
	   snd_pcm_format_t bitformat,
	   GError **error)
{
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	unsigned int sample_rate = audio_format->sample_rate;
	unsigned int channels = audio_format->channels;
	snd_pcm_uframes_t alsa_buffer_size;
	snd_pcm_uframes_t alsa_period_size;
	int err;
	const char *cmd = NULL;
	int retry = MPD_ALSA_RETRY_NR;
	unsigned int period_time, period_time_ro;
	unsigned int buffer_time;

	period_time_ro = period_time = ad->period_time;
configure_hw:
	/* configure HW params */
	snd_pcm_hw_params_alloca(&hwparams);
	cmd = "snd_pcm_hw_params_any";
	err = snd_pcm_hw_params_any(ad->pcm, hwparams);
	if (err < 0)
		goto error;

	if (ad->use_mmap) {
		err = snd_pcm_hw_params_set_access(ad->pcm, hwparams,
						   SND_PCM_ACCESS_MMAP_INTERLEAVED);
		if (err < 0) {
			g_warning("Cannot set mmap'ed mode on ALSA device \"%s\":  %s\n",
				  alsa_device(ad), snd_strerror(-err));
			g_warning("Falling back to direct write mode\n");
			ad->use_mmap = false;
		} else
			ad->writei = snd_pcm_mmap_writei;
	}

	if (!ad->use_mmap) {
		cmd = "snd_pcm_hw_params_set_access";
		err = snd_pcm_hw_params_set_access(ad->pcm, hwparams,
						   SND_PCM_ACCESS_RW_INTERLEAVED);
		if (err < 0)
			goto error;
		ad->writei = snd_pcm_writei;
	}

	err = snd_pcm_hw_params_set_format(ad->pcm, hwparams, bitformat);
	if (err == -EINVAL &&
	    byteswap_bitformat(bitformat) != SND_PCM_FORMAT_UNKNOWN) {
		err = snd_pcm_hw_params_set_format(ad->pcm, hwparams,
						   byteswap_bitformat(bitformat));
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to reverse-endian\n",
				alsa_device(ad), audio_format->bits);
			audio_format->reverse_endian = 1;
		}
	}
	if (err == -EINVAL && (audio_format->bits == 24 ||
			       audio_format->bits == 16)) {
		/* fall back to 32 bit, let pcm_convert.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcm, hwparams,
						   SND_PCM_FORMAT_S32);
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to 32 bit\n",
				alsa_device(ad), audio_format->bits);
			audio_format->bits = 32;
		}
	}
	if (err == -EINVAL && (audio_format->bits == 24 ||
			       audio_format->bits == 16)) {
		/* fall back to 32 bit, let pcm_convert.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcm, hwparams,
						   byteswap_bitformat(SND_PCM_FORMAT_S32));
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to 32 bit backward-endian\n",
				alsa_device(ad), audio_format->bits);
			audio_format->bits = 32;
			audio_format->reverse_endian = 1;
		}
	}

	if (err == -EINVAL && audio_format->bits != 16) {
		/* fall back to 16 bit, let pcm_convert.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcm, hwparams,
						   SND_PCM_FORMAT_S16);
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to 16 bit\n",
				alsa_device(ad), audio_format->bits);
			audio_format->bits = 16;
		}
	}
	if (err == -EINVAL && audio_format->bits != 16) {
		/* fall back to 16 bit, let pcm_convert.c do the conversion */
		err = snd_pcm_hw_params_set_format(ad->pcm, hwparams,
						   byteswap_bitformat(SND_PCM_FORMAT_S16));
		if (err == 0) {
			g_debug("ALSA device \"%s\": converting %u bit to 16 bit backward-endian\n",
				alsa_device(ad), audio_format->bits);
			audio_format->bits = 16;
			audio_format->reverse_endian = 1;
		}
	}

	if (err < 0) {
		g_set_error(error, alsa_output_quark(), err,
			    "ALSA device \"%s\" does not support %u bit audio: %s",
			    alsa_device(ad), audio_format->bits,
			    snd_strerror(-err));
		return false;
	}

	err = snd_pcm_hw_params_set_channels_near(ad->pcm, hwparams,
						  &channels);
	if (err < 0) {
		g_set_error(error, alsa_output_quark(), err,
			    "ALSA device \"%s\" does not support %i channels: %s",
			    alsa_device(ad), (int)audio_format->channels,
			    snd_strerror(-err));
		return false;
	}
	audio_format->channels = (int8_t)channels;

	err = snd_pcm_hw_params_set_rate_near(ad->pcm, hwparams,
					      &sample_rate, NULL);
	if (err < 0 || sample_rate == 0) {
		g_set_error(error, alsa_output_quark(), err,
			    "ALSA device \"%s\" does not support %u Hz audio",
			    alsa_device(ad), audio_format->sample_rate);
		return false;
	}
	audio_format->sample_rate = sample_rate;

	if (ad->buffer_time > 0) {
		buffer_time = ad->buffer_time;
		cmd = "snd_pcm_hw_params_set_buffer_time_near";
		err = snd_pcm_hw_params_set_buffer_time_near(ad->pcm, hwparams,
							     &buffer_time, NULL);
		if (err < 0)
			goto error;
	} else {
		err = snd_pcm_hw_params_get_buffer_time(hwparams, &buffer_time,
							NULL);
		if (err < 0)
			buffer_time = 0;
	}

	if (period_time_ro == 0 && buffer_time >= 10000) {
		period_time_ro = period_time = buffer_time / 4;

		g_debug("default period_time = buffer_time/4 = %u/4 = %u",
			buffer_time, period_time);
	}

	if (period_time_ro > 0) {
		period_time = period_time_ro;
		cmd = "snd_pcm_hw_params_set_period_time_near";
		err = snd_pcm_hw_params_set_period_time_near(ad->pcm, hwparams,
							     &period_time, NULL);
		if (err < 0)
			goto error;
	}

	cmd = "snd_pcm_hw_params";
	err = snd_pcm_hw_params(ad->pcm, hwparams);
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
	err = snd_pcm_sw_params_current(ad->pcm, swparams);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params_set_start_threshold";
	err = snd_pcm_sw_params_set_start_threshold(ad->pcm, swparams,
						    alsa_buffer_size -
						    alsa_period_size);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params_set_avail_min";
	err = snd_pcm_sw_params_set_avail_min(ad->pcm, swparams,
					      alsa_period_size);
	if (err < 0)
		goto error;

	cmd = "snd_pcm_sw_params";
	err = snd_pcm_sw_params(ad->pcm, swparams);
	if (err < 0)
		goto error;

	g_debug("buffer_size=%u period_size=%u",
		(unsigned)alsa_buffer_size, (unsigned)alsa_period_size);

	ad->period_frames = alsa_period_size;
	ad->period_position = 0;

	return true;

error:
	g_set_error(error, alsa_output_quark(), err,
		    "Error opening ALSA device \"%s\" (%s): %s",
		    alsa_device(ad), cmd, snd_strerror(-err));
	return false;
}

static bool
alsa_open(void *data, struct audio_format *audio_format, GError **error)
{
	struct alsa_data *ad = data;
	snd_pcm_format_t bitformat;
	int err;
	bool success;

	bitformat = get_bitformat(audio_format);
	if (bitformat == SND_PCM_FORMAT_UNKNOWN) {
		/* sample format is not supported by this plugin -
		   fall back to 16 bit samples */

		audio_format->bits = 16;
		bitformat = SND_PCM_FORMAT_S16;
	}

	err = snd_pcm_open(&ad->pcm, alsa_device(ad),
			   SND_PCM_STREAM_PLAYBACK, ad->mode);
	if (err < 0) {
		g_set_error(error, alsa_output_quark(), err,
			    "Failed to open ALSA device \"%s\": %s",
			    alsa_device(ad), snd_strerror(err));
		return false;
	}

	success = alsa_setup(ad, audio_format, bitformat, error);
	if (!success) {
		snd_pcm_close(ad->pcm);
		return false;
	}

	ad->frame_size = audio_format_frame_size(audio_format);

	return true;
}

static int
alsa_recover(struct alsa_data *ad, int err)
{
	if (err == -EPIPE) {
		g_debug("Underrun on ALSA device \"%s\"\n", alsa_device(ad));
	} else if (err == -ESTRPIPE) {
		g_debug("ALSA device \"%s\" was suspended\n", alsa_device(ad));
	}

	switch (snd_pcm_state(ad->pcm)) {
	case SND_PCM_STATE_PAUSED:
		err = snd_pcm_pause(ad->pcm, /* disable */ 0);
		break;
	case SND_PCM_STATE_SUSPENDED:
		err = snd_pcm_resume(ad->pcm);
		if (err == -EAGAIN)
			return 0;
		/* fall-through to snd_pcm_prepare: */
	case SND_PCM_STATE_SETUP:
	case SND_PCM_STATE_XRUN:
		ad->period_position = 0;
		err = snd_pcm_prepare(ad->pcm);
		break;
	case SND_PCM_STATE_DISCONNECTED:
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

static void
alsa_drain(void *data)
{
	struct alsa_data *ad = data;

	if (snd_pcm_state(ad->pcm) != SND_PCM_STATE_RUNNING)
		return;

	if (ad->period_position > 0) {
		/* generate some silence to finish the partial
		   period */
		snd_pcm_uframes_t nframes =
			ad->period_frames - ad->period_position;
		size_t nbytes = nframes * ad->frame_size;
		void *buffer = g_malloc(nbytes);
		snd_pcm_hw_params_t *params;
		snd_pcm_format_t format;
		unsigned channels;

		snd_pcm_hw_params_alloca(&params);
		snd_pcm_hw_params_current(ad->pcm, params);
		snd_pcm_hw_params_get_format(params, &format);
		snd_pcm_hw_params_get_channels(params, &channels);

		snd_pcm_format_set_silence(format, buffer, nframes * channels);
		ad->writei(ad->pcm, buffer, nframes);
		g_free(buffer);
	}

	snd_pcm_drain(ad->pcm);

	ad->period_position = 0;
}

static void
alsa_cancel(void *data)
{
	struct alsa_data *ad = data;

	ad->period_position = 0;

	snd_pcm_drop(ad->pcm);
}

static void
alsa_close(void *data)
{
	struct alsa_data *ad = data;

	snd_pcm_close(ad->pcm);
}

static size_t
alsa_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct alsa_data *ad = data;

	size /= ad->frame_size;

	while (true) {
		snd_pcm_sframes_t ret = ad->writei(ad->pcm, chunk, size);
		if (ret > 0) {
			ad->period_position = (ad->period_position + ret)
				% ad->period_frames;
			return ret * ad->frame_size;
		}

		if (ret < 0 && ret != -EAGAIN && ret != -EINTR &&
		    alsa_recover(ad, ret) < 0) {
			g_set_error(error, alsa_output_quark(), errno,
				    "%s", snd_strerror(-errno));
			return 0;
		}
	}
}

const struct audio_output_plugin alsaPlugin = {
	.name = "alsa",
	.test_default_device = alsa_test_default_device,
	.init = alsa_init,
	.finish = alsa_finish,
	.open = alsa_open,
	.play = alsa_play,
	.drain = alsa_drain,
	.cancel = alsa_cancel,
	.close = alsa_close,

	.mixer_plugin = &alsa_mixer_plugin,
};
