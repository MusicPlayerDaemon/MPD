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
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "fd_util.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "recorder"

struct recorder_output {
	/**
	 * The configured encoder plugin.
	 */
	struct encoder *encoder;

	/**
	 * The destination file name.
	 */
	const char *path;

	/**
	 * The destination file descriptor.
	 */
	int fd;

	/**
	 * The buffer for encoder_read().
	 */
	char buffer[32768];
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
recorder_output_quark(void)
{
	return g_quark_from_static_string("recorder_output");
}

static void *
recorder_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		     const struct config_param *param, GError **error_r)
{
	struct recorder_output *recorder = g_new(struct recorder_output, 1);
	const char *encoder_name;
	const struct encoder_plugin *encoder_plugin;

	/* read configuration */

	encoder_name = config_get_block_string(param, "encoder", "vorbis");
	encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == NULL) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		return NULL;
	}

	recorder->path = config_get_block_string(param, "path", NULL);
	if (recorder->path == NULL) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "'path' not configured");
		return NULL;
	}

	/* initialize encoder */

	recorder->encoder = encoder_init(encoder_plugin, param, error_r);
	if (recorder->encoder == NULL)
		return NULL;

	return recorder;
}

static void
recorder_output_finish(void *data)
{
	struct recorder_output *recorder = data;

	encoder_finish(recorder->encoder);
	g_free(recorder);
}

/**
 * Writes pending data from the encoder to the output file.
 */
static bool
recorder_output_encoder_to_file(struct recorder_output *recorder,
			      GError **error_r)
{
	size_t size = 0, position, nbytes;

	assert(recorder->fd >= 0);

	/* read from the encoder */

	size = encoder_read(recorder->encoder, recorder->buffer,
			    sizeof(recorder->buffer));
	if (size == 0)
		return true;

	/* write everything into the file */

	position = 0;
	while (true) {
		nbytes = write(recorder->fd, recorder->buffer + position,
			       size - position);
		if (nbytes > 0) {
			position += (size_t)nbytes;
			if (position >= size)
				return true;
		} else if (nbytes == 0) {
			/* shouldn't happen for files */
			g_set_error(error_r, recorder_output_quark(), 0,
				    "write() returned 0");
			return false;
		} else if (errno != EINTR) {
			g_set_error(error_r, recorder_output_quark(), 0,
				    "Failed to write to '%s': %s",
				    recorder->path, g_strerror(errno));
			return false;
		}
	}
}

static bool
recorder_output_open(void *data, struct audio_format *audio_format,
		     GError **error_r)
{
	struct recorder_output *recorder = data;
	bool success;

	/* create the output file */

	recorder->fd = open_cloexec(recorder->path, O_CREAT|O_WRONLY|O_TRUNC,
				    0666);
	if (recorder->fd < 0) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "Failed to create '%s': %s",
			    recorder->path, g_strerror(errno));
		return false;
	}

	/* open the encoder */

	success = encoder_open(recorder->encoder, audio_format, error_r);
	if (!success) {
		close(recorder->fd);
		unlink(recorder->path);
		return false;
	}

	return true;
}

static void
recorder_output_close(void *data)
{
	struct recorder_output *recorder = data;

	/* flush the encoder and write the rest to the file */

	if (encoder_flush(recorder->encoder, NULL))
		recorder_output_encoder_to_file(recorder, NULL);

	/* now really close everything */

	encoder_close(recorder->encoder);

	close(recorder->fd);
}

static size_t
recorder_output_play(void *data, const void *chunk, size_t size,
		     GError **error_r)
{
	struct recorder_output *recorder = data;

	return encoder_write(recorder->encoder, chunk, size, error_r) &&
		recorder_output_encoder_to_file(recorder, error_r)
		? size : 0;
}

const struct audio_output_plugin recorder_output_plugin = {
	.name = "recorder",
	.init = recorder_output_init,
	.finish = recorder_output_finish,
	.open = recorder_output_open,
	.close = recorder_output_close,
	.play = recorder_output_play,
};
