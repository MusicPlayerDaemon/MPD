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
#include "fd_util.h"

#include <glib.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "oss"

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <soundcard.h>
#else /* !(defined(__OpenBSD__) || defined(__NetBSD__) */
# include <sys/soundcard.h>
#endif /* !(defined(__OpenBSD__) || defined(__NetBSD__) */

#if G_BYTE_ORDER == G_BIG_ENDIAN
# define	AFMT_S16_MPD	 AFMT_S16_BE
#else
# define	AFMT_S16_MPD	 AFMT_S16_LE
#endif

struct oss_data {
	int fd;
	const char *device;
	struct audio_format audio_format;
	int bitFormat;
	int *supported[3];
	unsigned num_supported[3];
	int *unsupported[3];
	unsigned num_unsupported[3];
};

enum oss_support {
	OSS_SUPPORTED = 1,
	OSS_UNSUPPORTED = 0,
	OSS_UNKNOWN = -1,
};

enum oss_param {
	OSS_RATE = 0,
	OSS_CHANNELS = 1,
	OSS_BITS = 2,
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
oss_output_quark(void)
{
	return g_quark_from_static_string("oss_output");
}

static enum oss_param
oss_param_from_ioctl(unsigned param)
{
	enum oss_param idx = OSS_RATE;

	switch (param) {
	case SNDCTL_DSP_SPEED:
		idx = OSS_RATE;
		break;
	case SNDCTL_DSP_CHANNELS:
		idx = OSS_CHANNELS;
		break;
	case SNDCTL_DSP_SAMPLESIZE:
		idx = OSS_BITS;
		break;
	}

	return idx;
}

static bool
oss_find_supported_param(struct oss_data *od, unsigned param, int val)
{
	enum oss_param idx = oss_param_from_ioctl(param);

	for (unsigned i = 0; i < od->num_supported[idx]; i++)
		if (od->supported[idx][i] == val)
			return true;

	return false;
}

static bool
oss_can_convert(int idx, int val)
{
	switch (idx) {
	case OSS_BITS:
		if (val != 16)
			return false;
		break;
	case OSS_CHANNELS:
		if (val != 2)
			return false;
		break;
	}

	return true;
}

static int
oss_get_supported_param(struct oss_data *od, unsigned param, int val)
{
	enum oss_param idx = oss_param_from_ioctl(param);
	int ret = -1;
	int least = val;
	int diff;

	for (unsigned i = 0; i < od->num_supported[idx]; i++) {
		diff = od->supported[idx][i] - val;
		if (diff < 0)
			diff = -diff;
		if (diff < least) {
			if (!oss_can_convert(idx, od->supported[idx][i]))
				continue;

			least = diff;
			ret = od->supported[idx][i];
		}
	}

	return ret;
}

static bool
oss_find_unsupported_param(struct oss_data *od, unsigned param, int val)
{
	enum oss_param idx = oss_param_from_ioctl(param);

	for (unsigned i = 0; i < od->num_unsupported[idx]; i++) {
		if (od->unsupported[idx][i] == val)
			return true;
	}

	return false;
}

static void
oss_add_supported_param(struct oss_data *od, unsigned param, int val)
{
	enum oss_param idx = oss_param_from_ioctl(param);

	od->num_supported[idx]++;
	od->supported[idx] = g_realloc(od->supported[idx],
				       od->num_supported[idx] * sizeof(int));
	od->supported[idx][od->num_supported[idx] - 1] = val;
}

static void
oss_add_unsupported_param(struct oss_data *od, unsigned param, int val)
{
	enum oss_param idx = oss_param_from_ioctl(param);

	od->num_unsupported[idx]++;
	od->unsupported[idx] = g_realloc(od->unsupported[idx],
					 od->num_unsupported[idx] *
					 sizeof(int));
	od->unsupported[idx][od->num_unsupported[idx] - 1] = val;
}

static void
oss_remove_supported_param(struct oss_data *od, unsigned param, int val)
{
	unsigned j = 0;
	enum oss_param idx = oss_param_from_ioctl(param);

	for (unsigned i = 0; i < od->num_supported[idx] - 1; i++) {
		if (od->supported[idx][i] == val)
			j = 1;
		od->supported[idx][i] = od->supported[idx][i + j];
	}

	od->num_supported[idx]--;
	od->supported[idx] = g_realloc(od->supported[idx],
				       od->num_supported[idx] * sizeof(int));
}

static void
oss_remove_unsupported_param(struct oss_data *od, unsigned param, int val)
{
	unsigned j = 0;
	enum oss_param idx = oss_param_from_ioctl(param);

	for (unsigned i = 0; i < od->num_unsupported[idx] - 1; i++) {
		if (od->unsupported[idx][i] == val)
			j = 1;
		od->unsupported[idx][i] = od->unsupported[idx][i + j];
	}

	od->num_unsupported[idx]--;
	od->unsupported[idx] = g_realloc(od->unsupported[idx],
					 od->num_unsupported[idx] *
					 sizeof(int));
}

static enum oss_support
oss_param_is_supported(struct oss_data *od, unsigned param, int val)
{
	if (oss_find_supported_param(od, param, val))
		return OSS_SUPPORTED;
	if (oss_find_unsupported_param(od, param, val))
		return OSS_UNSUPPORTED;
	return OSS_UNKNOWN;
}

static void
oss_set_supported(struct oss_data *od, unsigned param, int val)
{
	enum oss_support supported = oss_param_is_supported(od, param, val);

	if (supported == OSS_SUPPORTED)
		return;

	if (supported == OSS_UNSUPPORTED)
		oss_remove_unsupported_param(od, param, val);

	oss_add_supported_param(od, param, val);
}

static void
oss_set_unsupported(struct oss_data *od, unsigned param, int val)
{
	enum oss_support supported = oss_param_is_supported(od, param, val);

	if (supported == OSS_UNSUPPORTED)
		return;

	if (supported == OSS_SUPPORTED)
		oss_remove_supported_param(od, param, val);

	oss_add_unsupported_param(od, param, val);
}

static struct oss_data *
oss_data_new(void)
{
	struct oss_data *ret = g_new(struct oss_data, 1);

	ret->device = NULL;
	ret->fd = -1;

	ret->supported[OSS_RATE] = NULL;
	ret->supported[OSS_CHANNELS] = NULL;
	ret->supported[OSS_BITS] = NULL;
	ret->unsupported[OSS_RATE] = NULL;
	ret->unsupported[OSS_CHANNELS] = NULL;
	ret->unsupported[OSS_BITS] = NULL;

	ret->num_supported[OSS_RATE] = 0;
	ret->num_supported[OSS_CHANNELS] = 0;
	ret->num_supported[OSS_BITS] = 0;
	ret->num_unsupported[OSS_RATE] = 0;
	ret->num_unsupported[OSS_CHANNELS] = 0;
	ret->num_unsupported[OSS_BITS] = 0;

	oss_set_supported(ret, SNDCTL_DSP_SPEED, 48000);
	oss_set_supported(ret, SNDCTL_DSP_SPEED, 44100);
	oss_set_supported(ret, SNDCTL_DSP_CHANNELS, 2);
	oss_set_supported(ret, SNDCTL_DSP_SAMPLESIZE, 16);

	return ret;
}

static void
oss_data_free(struct oss_data *od)
{
	g_free(od->supported[OSS_RATE]);
	g_free(od->supported[OSS_CHANNELS]);
	g_free(od->supported[OSS_BITS]);
	g_free(od->unsupported[OSS_RATE]);
	g_free(od->unsupported[OSS_CHANNELS]);
	g_free(od->unsupported[OSS_BITS]);

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

static int
oss_set_param(struct oss_data *od, unsigned param, int *value)
{
	int val = *value;
	int copy;
	enum oss_support supported = oss_param_is_supported(od, param, val);

	do {
		if (supported == OSS_UNSUPPORTED) {
			val = oss_get_supported_param(od, param, val);
			if (copy < 0)
				return -1;
		}
		copy = val;
		if (ioctl(od->fd, param, &copy)) {
			oss_set_unsupported(od, param, val);
			supported = OSS_UNSUPPORTED;
		} else {
			if (supported == OSS_UNKNOWN) {
				oss_set_supported(od, param, val);
				supported = OSS_SUPPORTED;
			}
			val = copy;
		}
	} while (supported == OSS_UNSUPPORTED);

	*value = val;

	return 0;
}

static void
oss_close(struct oss_data *od)
{
	if (od->fd >= 0)
		while (close(od->fd) && errno == EINTR) ;
	od->fd = -1;
}

/**
 * Sets up the OSS device which was opened before.
 */
static bool
oss_setup(struct oss_data *od, GError **error)
{
	int tmp;

	tmp = od->audio_format.channels;
	if (oss_set_param(od, SNDCTL_DSP_CHANNELS, &tmp)) {
		g_set_error(error, oss_output_quark(), errno,
			    "OSS device \"%s\" does not support %u channels: %s",
			    od->device, od->audio_format.channels,
			    strerror(errno));
		return false;
	}
	od->audio_format.channels = tmp;

	tmp = od->audio_format.sample_rate;
	if (oss_set_param(od, SNDCTL_DSP_SPEED, &tmp)) {
		g_set_error(error, oss_output_quark(), errno,
			    "OSS device \"%s\" does not support %u Hz audio: %s",
			    od->device, od->audio_format.sample_rate,
			    strerror(errno));
		return false;
	}
	od->audio_format.sample_rate = tmp;

	switch (od->audio_format.bits) {
	case 8:
		tmp = AFMT_S8;
		break;
	case 16:
		tmp = AFMT_S16_MPD;
		break;

	default:
		/* not supported by OSS - fall back to 16 bit */
		od->audio_format.bits = 16;
		tmp = AFMT_S16_MPD;
		break;
	}

	if (oss_set_param(od, SNDCTL_DSP_SAMPLESIZE, &tmp)) {
		g_set_error(error, oss_output_quark(), errno,
			    "OSS device \"%s\" does not support %u bit audio: %s",
			    od->device, tmp, strerror(errno));
		return false;
	}

	return true;
}

static bool
oss_open(struct oss_data *od, GError **error)
{
	bool success;

	od->fd = open_cloexec(od->device, O_WRONLY, 0);
	if (od->fd < 0) {
		g_set_error(error, oss_output_quark(), errno,
			    "Error opening OSS device \"%s\": %s",
			    od->device, strerror(errno));
		return false;
	}

	success = oss_setup(od, error);
	if (!success) {
		oss_close(od);
		return false;
	}

	return true;
}

static bool
oss_output_open(void *data, struct audio_format *audio_format, GError **error)
{
	bool ret;
	struct oss_data *od = data;

	od->audio_format = *audio_format;

	ret = oss_open(od, error);
	if (!ret)
		return false;

	*audio_format = od->audio_format;

	return ret;
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
	if (od->fd < 0 && !oss_open(od, error))
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
