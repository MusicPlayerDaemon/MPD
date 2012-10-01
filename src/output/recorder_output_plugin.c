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
#include "recorder_output_plugin.h"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "fd_util.h"
#include "open.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "recorder"

struct recorder_output {
	struct audio_output base;

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

static struct audio_output *
recorder_output_init(const struct config_param *param, GError **error_r)
{
	struct recorder_output *recorder = g_new(struct recorder_output, 1);
	if (!ao_base_init(&recorder->base, &recorder_output_plugin, param,
			  error_r)) {
		g_free(recorder);
		return NULL;
	}

	/* read configuration */

	const char *encoder_name =
		config_get_block_string(param, "encoder", "vorbis");
	const struct encoder_plugin *encoder_plugin =
		encoder_plugin_get(encoder_name);
	if (encoder_plugin == NULL) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		goto failure;
	}

	recorder->path = config_get_block_string(param, "path", NULL);
	if (recorder->path == NULL) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "'path' not configured");
		goto failure;
	}

	/* initialize encoder */

	recorder->encoder = encoder_init(encoder_plugin, param, error_r);
	if (recorder->encoder == NULL)
		goto failure;

	return &recorder->base;

failure:
	ao_base_finish(&recorder->base);
	g_free(recorder);
	return NULL;
}

static void
recorder_output_finish(struct audio_output *ao)
{
	struct recorder_output *recorder = (struct recorder_output *)ao;

	encoder_finish(recorder->encoder);
	ao_base_finish(&recorder->base);
	g_free(recorder);
}

static bool
recorder_write_to_file(struct recorder_output *recorder,
		       const void *_data, size_t length,
		       GError **error_r)
{
	assert(length > 0);

	const int fd = recorder->fd;

	const uint8_t *data = (const uint8_t *)_data, *end = data + length;

	while (true) {
		ssize_t nbytes = write(fd, data, end - data);
		if (nbytes > 0) {
			data += nbytes;
			if (data == end)
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

/**
 * Writes pending data from the encoder to the output file.
 */
static bool
recorder_output_encoder_to_file(struct recorder_output *recorder,
				GError **error_r)
{
	assert(recorder->fd >= 0);

	while (true) {
		/* read from the encoder */

		size_t size = encoder_read(recorder->encoder, recorder->buffer,
					   sizeof(recorder->buffer));
		if (size == 0)
			return true;

		/* write everything into the file */

		if (!recorder_write_to_file(recorder, recorder->buffer, size,
					    error_r))
			return false;
	}
}

static bool
recorder_output_open(struct audio_output *ao,
		     struct audio_format *audio_format,
		     GError **error_r)
{
	struct recorder_output *recorder = (struct recorder_output *)ao;

	/* create the output file */

	recorder->fd = open_cloexec(recorder->path,
				    O_CREAT|O_WRONLY|O_TRUNC|O_BINARY,
				    0666);
	if (recorder->fd < 0) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "Failed to create '%s': %s",
			    recorder->path, g_strerror(errno));
		return false;
	}

	/* open the encoder */

	if (!encoder_open(recorder->encoder, audio_format, error_r)) {
		close(recorder->fd);
		unlink(recorder->path);
		return false;
	}

	if (!recorder_output_encoder_to_file(recorder, error_r)) {
		encoder_close(recorder->encoder);
		close(recorder->fd);
		unlink(recorder->path);
		return false;
	}

	return true;
}

static void
recorder_output_close(struct audio_output *ao)
{
	struct recorder_output *recorder = (struct recorder_output *)ao;

	/* flush the encoder and write the rest to the file */

	if (encoder_end(recorder->encoder, NULL))
		recorder_output_encoder_to_file(recorder, NULL);

	/* now really close everything */

	encoder_close(recorder->encoder);

	close(recorder->fd);
}

static size_t
recorder_output_play(struct audio_output *ao, const void *chunk, size_t size,
		     GError **error_r)
{
	struct recorder_output *recorder = (struct recorder_output *)ao;

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
