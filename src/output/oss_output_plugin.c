/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "fd_util.h"

#include <glib.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "oss"

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

/* We got bug reports from FreeBSD users who said that the two 24 bit
   formats generate white noise on FreeBSD, but 32 bit works.  This is
   a workaround until we know what exactly is expected by the kernel
   audio drivers. */
#ifndef __linux__
#undef AFMT_S24_PACKED
#undef AFMT_S24_NE
#endif

struct oss_data {
	int fd;
	const char *device;

	/**
	 * The current input audio format.  This is needed to reopen
	 * the device after cancel().
	 */
	struct audio_format audio_format;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
oss_output_quark(void)
{
	return g_quark_from_static_string("oss_output");
}

static struct oss_data *
oss_data_new(void)
{
	struct oss_data *ret = g_new(struct oss_data, 1);

	ret->device = NULL;
	ret->fd = -1;

	return ret;
}

static void
oss_data_free(struct oss_data *od)
{
	g_free(od);
}

enum oss_stat {
	OSS_STAT_NO_ERROR = 0,
	OSS_STAT_NOT_CHAR_DEV = -1,
	OSS_STAT_NO_PERMS = -2,
	OSS_STAT_DOESN_T_EXIST = -3,
	OSS_STAT_OTHER = -4,
};

static enum oss_stat
oss_stat_device(const char *device, int *errno_r)
{
	struct stat st;

	if (0 == stat(device, &st)) {
		if (!S_ISCHR(st.st_mode)) {
			return OSS_STAT_NOT_CHAR_DEV;
		}
	} else {
		*errno_r = errno;

		switch (errno) {
		case ENOENT:
		case ENOTDIR:
			return OSS_STAT_DOESN_T_EXIST;
		case EACCES:
			return OSS_STAT_NO_PERMS;
		default:
			return OSS_STAT_OTHER;
		}
	}

	return OSS_STAT_NO_ERROR;
}

static const char *default_devices[] = { "/dev/sound/dsp", "/dev/dsp" };

static bool
oss_output_test_default_device(void)
{
	int fd, i;

	for (i = G_N_ELEMENTS(default_devices); --i >= 0; ) {
		fd = open_cloexec(default_devices[i], O_WRONLY, 0);

		if (fd >= 0) {
			close(fd);
			return true;
		}
		g_warning("Error opening OSS device \"%s\": %s\n",
			  default_devices[i], strerror(errno));
	}

	return false;
}

static void *
oss_open_default(GError **error)
{
	int i;
	int err[G_N_ELEMENTS(default_devices)];
	enum oss_stat ret[G_N_ELEMENTS(default_devices)];

	for (i = G_N_ELEMENTS(default_devices); --i >= 0; ) {
		ret[i] = oss_stat_device(default_devices[i], &err[i]);
		if (ret[i] == OSS_STAT_NO_ERROR) {
			struct oss_data *od = oss_data_new();
			od->device = default_devices[i];
			return od;
		}
	}

	for (i = G_N_ELEMENTS(default_devices); --i >= 0; ) {
		const char *dev = default_devices[i];
		switch(ret[i]) {
		case OSS_STAT_NO_ERROR:
			/* never reached */
			break;
		case OSS_STAT_DOESN_T_EXIST:
			g_warning("%s not found\n", dev);
			break;
		case OSS_STAT_NOT_CHAR_DEV:
			g_warning("%s is not a character device\n", dev);
			break;
		case OSS_STAT_NO_PERMS:
			g_warning("%s: permission denied\n", dev);
			break;
		case OSS_STAT_OTHER:
			g_warning("Error accessing %s: %s\n",
				  dev, strerror(err[i]));
		}
	}

	g_set_error(error, oss_output_quark(), 0,
		    "error trying to open default OSS device");
	return NULL;
}

static void *
oss_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		const struct config_param *param,
		GError **error)
{
	const char *device = config_get_block_string(param, "device", NULL);
	if (device != NULL) {
		struct oss_data *od = oss_data_new();
		od->device = device;
		return od;
	}

	return oss_open_default(error);
}

static void
oss_output_finish(void *data)
{
	struct oss_data *od = data;

	oss_data_free(od);
}

static void
oss_close(struct oss_data *od)
{
	if (od->fd >= 0)
		close(od->fd);
	od->fd = -1;
}

/**
 * A tri-state type for oss_try_ioctl().
 */
enum oss_setup_result {
	SUCCESS,
	ERROR,
	UNSUPPORTED,
};

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure returns ERROR and allocates a GError.
 */
static enum oss_setup_result
oss_try_ioctl_r(int fd, unsigned long request, int *value_r,
		const char *msg, GError **error_r)
{
	assert(fd >= 0);
	assert(value_r != NULL);
	assert(msg != NULL);
	assert(error_r == NULL || *error_r == NULL);

	int ret = ioctl(fd, request, value_r);
	if (ret >= 0)
		return SUCCESS;

	if (errno == EINVAL)
		return UNSUPPORTED;

	g_set_error(error_r, oss_output_quark(), errno,
		    "%s: %s", msg, g_strerror(errno));
	return ERROR;
}

/**
 * Invoke an ioctl on the OSS file descriptor.  On success, SUCCESS is
 * returned.  If the parameter is not supported, UNSUPPORTED is
 * returned.  Any other failure returns ERROR and allocates a GError.
 */
static enum oss_setup_result
oss_try_ioctl(int fd, unsigned long request, int value,
	      const char *msg, GError **error_r)
{
	return oss_try_ioctl_r(fd, request, &value, msg, error_r);
}

/**
 * Set up the channel number, and attempts to find alternatives if the
 * specified number is not supported.
 */
static bool
oss_setup_channels(int fd, struct audio_format *audio_format, GError **error_r)
{
	const char *const msg = "Failed to set channel count";
	int channels = audio_format->channels;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels, msg, error_r);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_channel_count(channels))
		    break;

		audio_format->channels = channels;
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	for (unsigned i = 1; i < 2; ++i) {
		if (i == audio_format->channels)
			/* don't try that again */
			continue;

		channels = i;
		result = oss_try_ioctl_r(fd, SNDCTL_DSP_CHANNELS, &channels,
					 msg, error_r);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_channel_count(channels))
			    break;

			audio_format->channels = channels;
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	g_set_error(error_r, oss_output_quark(), EINVAL, "%s", msg);
	return false;
}

/**
 * Set up the sample rate, and attempts to find alternatives if the
 * specified sample rate is not supported.
 */
static bool
oss_setup_sample_rate(int fd, struct audio_format *audio_format,
		      GError **error_r)
{
	const char *const msg = "Failed to set sample rate";
	int sample_rate = audio_format->sample_rate;
	enum oss_setup_result result =
		oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
				msg, error_r);
	switch (result) {
	case SUCCESS:
		if (!audio_valid_sample_rate(sample_rate))
			break;

		audio_format->sample_rate = sample_rate;
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	static const int sample_rates[] = { 48000, 44100, 0 };
	for (unsigned i = 0; sample_rates[i] != 0; ++i) {
		sample_rate = sample_rates[i];
		if (sample_rate == (int)audio_format->sample_rate)
			continue;

		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SPEED, &sample_rate,
					 msg, error_r);
		switch (result) {
		case SUCCESS:
			if (!audio_valid_sample_rate(sample_rate))
				break;

			audio_format->sample_rate = sample_rate;
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	g_set_error(error_r, oss_output_quark(), EINVAL, "%s", msg);
	return false;
}

/**
 * Convert a MPD sample format to its OSS counterpart.  Returns
 * AFMT_QUERY if there is no direct counterpart.
 */
static int
sample_format_to_oss(enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
		return AFMT_QUERY;

	case SAMPLE_FORMAT_S8:
		return AFMT_S8;

	case SAMPLE_FORMAT_S16:
		return AFMT_S16_NE;

	case SAMPLE_FORMAT_S24:
#ifdef AFMT_S24_PACKED
		return AFMT_S24_PACKED;
#else
		return AFMT_QUERY;
#endif

	case SAMPLE_FORMAT_S24_P32:
#ifdef AFMT_S24_NE
		return AFMT_S24_NE;
#else
		return AFMT_QUERY;
#endif

	case SAMPLE_FORMAT_S32:
#ifdef AFMT_S32_NE
		return AFMT_S32_NE;
#else
		return AFMT_QUERY;
#endif
	}

	return AFMT_QUERY;
}

/**
 * Convert an OSS sample format to its MPD counterpart.  Returns
 * SAMPLE_FORMAT_UNDEFINED if there is no direct counterpart.
 */
static enum sample_format
sample_format_from_oss(int format)
{
	switch (format) {
	case AFMT_S8:
		return SAMPLE_FORMAT_S8;

	case AFMT_S16_NE:
		return SAMPLE_FORMAT_S16;

#ifdef AFMT_S24_PACKED
	case AFMT_S24_PACKED:
		return SAMPLE_FORMAT_S24;
#endif

#ifdef AFMT_S24_NE
	case AFMT_S24_NE:
		return SAMPLE_FORMAT_S24_P32;
#endif

#ifdef AFMT_S32_NE
	case AFMT_S32_NE:
		return SAMPLE_FORMAT_S32;
#endif

	default:
		return SAMPLE_FORMAT_UNDEFINED;
	}
}

/**
 * Set up the sample format, and attempts to find alternatives if the
 * specified format is not supported.
 */
static bool
oss_setup_sample_format(int fd, struct audio_format *audio_format,
			GError **error_r)
{
	const char *const msg = "Failed to set sample format";
	int oss_format = sample_format_to_oss(audio_format->format);
	enum oss_setup_result result = oss_format != AFMT_QUERY
		? oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
				  &oss_format, msg, error_r)
		: UNSUPPORTED;
	enum sample_format mpd_format;
	switch (result) {
	case SUCCESS:
		mpd_format = sample_format_from_oss(oss_format);
		if (mpd_format == SAMPLE_FORMAT_UNDEFINED)
			break;

		audio_format->format = mpd_format;

#ifdef AFMT_S24_PACKED
		if (oss_format == AFMT_S24_PACKED)
			audio_format->reverse_endian =
				G_BYTE_ORDER != G_LITTLE_ENDIAN;
#endif
		return true;

	case ERROR:
		return false;

	case UNSUPPORTED:
		break;
	}

	/* the requested sample format is not available - probe for
	   other formats supported by MPD */

	static const enum sample_format sample_formats[] = {
		SAMPLE_FORMAT_S24_P32,
		SAMPLE_FORMAT_S32,
		SAMPLE_FORMAT_S24,
		SAMPLE_FORMAT_S16,
		SAMPLE_FORMAT_S8,
		SAMPLE_FORMAT_UNDEFINED /* sentinel */
	};

	for (unsigned i = 0; sample_formats[i] != SAMPLE_FORMAT_UNDEFINED; ++i) {
		mpd_format = sample_formats[i];
		if (mpd_format == audio_format->format)
			/* don't try that again */
			continue;

		oss_format = sample_format_to_oss(mpd_format);
		if (oss_format == AFMT_QUERY)
			/* not supported by this OSS version */
			continue;

		result = oss_try_ioctl_r(fd, SNDCTL_DSP_SAMPLESIZE,
					 &oss_format, msg, error_r);
		switch (result) {
		case SUCCESS:
			mpd_format = sample_format_from_oss(oss_format);
			if (mpd_format == SAMPLE_FORMAT_UNDEFINED)
				break;

			audio_format->format = mpd_format;

#ifdef AFMT_S24_PACKED
			if (oss_format == AFMT_S24_PACKED)
				audio_format->reverse_endian =
					G_BYTE_ORDER != G_LITTLE_ENDIAN;
#endif
			return true;

		case ERROR:
			return false;

		case UNSUPPORTED:
			break;
		}
	}

	g_set_error(error_r, oss_output_quark(), EINVAL, "%s", msg);
	return false;
}

/**
 * Sets up the OSS device which was opened before.
 */
static bool
oss_setup(struct oss_data *od, struct audio_format *audio_format,
	  GError **error_r)
{
	return oss_setup_channels(od->fd, audio_format, error_r) &&
		oss_setup_sample_rate(od->fd, audio_format, error_r) &&
		oss_setup_sample_format(od->fd, audio_format, error_r);
}

/**
 * Reopen the device with the saved audio_format, without any probing.
 */
static bool
oss_reopen(struct oss_data *od, GError **error_r)
{
	assert(od->fd < 0);

	od->fd = open_cloexec(od->device, O_WRONLY, 0);
	if (od->fd < 0) {
		g_set_error(error_r, oss_output_quark(), errno,
			    "Error opening OSS device \"%s\": %s",
			    od->device, strerror(errno));
		return false;
	}

	enum oss_setup_result result;

	const char *const msg1 = "Failed to set channel count";
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_CHANNELS,
			       od->audio_format.channels, msg1, error_r);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			g_set_error(error_r, oss_output_quark(), EINVAL,
				    "%s", msg1);
		return false;
	}

	const char *const msg2 = "Failed to set sample rate";
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_SPEED,
			       od->audio_format.sample_rate, msg2, error_r);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			g_set_error(error_r, oss_output_quark(), EINVAL,
				    "%s", msg2);
		return false;
	}

	const char *const msg3 = "Failed to set sample format";
	assert(sample_format_to_oss(od->audio_format.format) != AFMT_QUERY);
	result = oss_try_ioctl(od->fd, SNDCTL_DSP_SAMPLESIZE,
			       sample_format_to_oss(od->audio_format.format),
			       msg3, error_r);
	if (result != SUCCESS) {
		oss_close(od);
		if (result == UNSUPPORTED)
			g_set_error(error_r, oss_output_quark(), EINVAL,
				    "%s", msg3);
		return false;
	}

	return true;
}

static bool
oss_output_open(void *data, struct audio_format *audio_format, GError **error)
{
	struct oss_data *od = data;

	od->fd = open_cloexec(od->device, O_WRONLY, 0);
	if (od->fd < 0) {
		g_set_error(error, oss_output_quark(), errno,
			    "Error opening OSS device \"%s\": %s",
			    od->device, strerror(errno));
		return false;
	}

	if (!oss_setup(od, audio_format, error)) {
		oss_close(od);
		return false;
	}

	od->audio_format = *audio_format;
	return true;
}

static void
oss_output_close(void *data)
{
	struct oss_data *od = data;

	oss_close(od);
}

static void
oss_output_cancel(void *data)
{
	struct oss_data *od = data;

	if (od->fd >= 0) {
		ioctl(od->fd, SNDCTL_DSP_RESET, 0);
		oss_close(od);
	}
}

static size_t
oss_output_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct oss_data *od = data;
	ssize_t ret;

	/* reopen the device since it was closed by dropBufferedAudio */
	if (od->fd < 0 && !oss_reopen(od, error))
		return 0;

	while (true) {
		ret = write(od->fd, chunk, size);
		if (ret > 0)
			return (size_t)ret;

		if (ret < 0 && errno != EINTR) {
			g_set_error(error, oss_output_quark(), errno,
				    "Write error on %s: %s",
				    od->device, strerror(errno));
			return 0;
		}
	}
}

const struct audio_output_plugin oss_output_plugin = {
	.name = "oss",
	.test_default_device = oss_output_test_default_device,
	.init = oss_output_init,
	.finish = oss_output_finish,
	.open = oss_output_open,
	.close = oss_output_close,
	.play = oss_output_play,
	.cancel = oss_output_cancel,

	.mixer_plugin = &oss_mixer_plugin,
};
