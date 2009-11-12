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
#include "fd_util.h"

#include <glib.h>

#include <sys/audio.h>
#include <sys/stropts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "solaris_output"

struct solaris_output {
	/* configuration */
	const char *device;

	int fd;
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
solaris_output_quark(void)
{
	return g_quark_from_static_string("solaris_output");
}

static bool
solaris_output_test_default_device(void)
{
	struct stat st;

	return stat("/dev/audio", &st) == 0 && S_ISCHR(st.st_mode) &&
		access("/dev/audio", W_OK) == 0;
}

static void *
solaris_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		    const struct config_param *param,
		    G_GNUC_UNUSED GError **error)
{
	struct solaris_output *so = g_new(struct solaris_output, 1);

	so->device = config_get_block_string(param, "device", "/dev/audio");

	return so;
}

static void
solaris_output_finish(void *data)
{
	struct solaris_output *so = data;

	g_free(so);
}

static bool
solaris_output_open(void *data, struct audio_format *audio_format,
		    GError **error)
{
	struct solaris_output *so = data;
	struct audio_info info;
	int ret, flags;

	/* support only 16 bit mono/stereo for now; nothing else has
	   been tested */
	audio_format->bits = 16;

	/* open the device in non-blocking mode */

	so->fd = open_cloexec(so->device, O_WRONLY|O_NONBLOCK);
	if (so->fd < 0) {
		g_set_error(error, solaris_output_quark(), errno,
			    "Failed to open %s: %s",
			    so->device, g_strerror(errno));
		return false;
	}

	/* restore blocking mode */

	flags = fcntl(so->fd, F_GETFL);
	if (flags > 0 && (flags & O_NONBLOCK) != 0)
		fcntl(so->fd, F_SETFL, flags & ~O_NONBLOCK);

	/* configure the audio device */

	ret = ioctl(so->fd, AUDIO_GETINFO, &info);
	if (ret < 0) {
		g_set_error(error, solaris_output_quark(), errno,
			    "AUDIO_GETINFO failed: %s", g_strerror(errno));
		close(so->fd);
		return false;
	}

	info.play.sample_rate = audio_format->sample_rate;
	info.play.channels = audio_format->channels;
	info.play.precision = audio_format->bits;
	info.play.encoding = AUDIO_ENCODING_LINEAR;

	ret = ioctl(so->fd, AUDIO_SETINFO, &info);
	if (ret < 0) {
		g_set_error(error, solaris_output_quark(), errno,
			    "AUDIO_SETINFO failed: %s", g_strerror(errno));
		close(so->fd);
		return false;
	}

	return true;
}

static void
solaris_output_close(void *data)
{
	struct solaris_output *so = data;

	close(so->fd);
}

static size_t
solaris_output_play(void *data, const void *chunk, size_t size, GError **error)
{
	struct solaris_output *so = data;
	ssize_t nbytes;

	nbytes = write(so->fd, chunk, size);
	if (nbytes <= 0) {
		g_set_error(error, solaris_output_quark(), errno,
			    "Write failed: %s", g_strerror(errno));
		return 0;
	}

	return nbytes;
}

static void
solaris_output_cancel(void *data)
{
	struct solaris_output *so = data;

	ioctl(so->fd, I_FLUSH);
}

const struct audio_output_plugin solaris_output_plugin = {
	.name = "solaris",
	.test_default_device = solaris_output_test_default_device,
	.init = solaris_output_init,
	.finish = solaris_output_finish,
	.open = solaris_output_open,
	.close = solaris_output_close,
	.play = solaris_output_play,
	.cancel = solaris_output_cancel,
};
