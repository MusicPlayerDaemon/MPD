/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "RecorderOutputPlugin.hxx"
#include "OutputAPI.hxx"
#include "EncoderPlugin.hxx"
#include "EncoderList.hxx"
#include "fd_util.h"
#include "open.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "recorder"

struct RecorderOutput {
	struct audio_output base;

	/**
	 * The configured encoder plugin.
	 */
	Encoder *encoder;

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

	bool Initialize(const config_param &param, GError **error_r) {
		return ao_base_init(&base, &recorder_output_plugin, param,
				    error_r);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}

	bool Configure(const config_param &param, GError **error_r);

	bool WriteToFile(const void *data, size_t length, GError **error_r);

	/**
	 * Writes pending data from the encoder to the output file.
	 */
	bool EncoderToFile(GError **error_r);
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
recorder_output_quark(void)
{
	return g_quark_from_static_string("recorder_output");
}

inline bool
RecorderOutput::Configure(const config_param &param, GError **error_r)
{
	/* read configuration */

	const char *encoder_name =
		param.GetBlockValue("encoder", "vorbis");
	const auto encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == nullptr) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "No such encoder: %s", encoder_name);
		return false;
	}

	path = param.GetBlockValue("path");
	if (path == nullptr) {
		g_set_error(error_r, recorder_output_quark(), 0,
			    "'path' not configured");
		return false;
	}

	/* initialize encoder */

	encoder = encoder_init(*encoder_plugin, &param, error_r);
	if (encoder == nullptr)
		return false;

	return true;
}

static audio_output *
recorder_output_init(const config_param &param, GError **error_r)
{
	RecorderOutput *recorder = new RecorderOutput();

	if (!recorder->Initialize(param, error_r)) {
		delete recorder;
		return nullptr;
	}

	if (!recorder->Configure(param, error_r)) {
		recorder->Deinitialize();
		delete recorder;
		return nullptr;
	}

	return &recorder->base;
}

static void
recorder_output_finish(struct audio_output *ao)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	encoder_finish(recorder->encoder);
	recorder->Deinitialize();
	delete recorder;
}

inline bool
RecorderOutput::WriteToFile(const void *_data, size_t length, GError **error_r)
{
	assert(length > 0);

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
				    path, g_strerror(errno));
			return false;
		}
	}
}

inline bool
RecorderOutput::EncoderToFile(GError **error_r)
{
	assert(fd >= 0);

	while (true) {
		/* read from the encoder */

		size_t size = encoder_read(encoder, buffer, sizeof(buffer));
		if (size == 0)
			return true;

		/* write everything into the file */

		if (!WriteToFile(buffer, size, error_r))
			return false;
	}
}

static bool
recorder_output_open(struct audio_output *ao,
		     AudioFormat &audio_format,
		     GError **error_r)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

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

	if (!recorder->EncoderToFile(error_r)) {
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
	RecorderOutput *recorder = (RecorderOutput *)ao;

	/* flush the encoder and write the rest to the file */

	if (encoder_end(recorder->encoder, nullptr))
		recorder->EncoderToFile(nullptr);

	/* now really close everything */

	encoder_close(recorder->encoder);

	close(recorder->fd);
}

static size_t
recorder_output_play(struct audio_output *ao, const void *chunk, size_t size,
		     GError **error_r)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	return encoder_write(recorder->encoder, chunk, size, error_r) &&
		recorder->EncoderToFile(error_r)
		? size : 0;
}

const struct audio_output_plugin recorder_output_plugin = {
	"recorder",
	nullptr,
	recorder_output_init,
	recorder_output_finish,
	nullptr,
	nullptr,
	recorder_output_open,
	recorder_output_close,
	nullptr,
	nullptr,
	recorder_output_play,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};
