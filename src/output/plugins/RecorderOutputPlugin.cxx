/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "../OutputAPI.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderList.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/fd_util.h"
#include "open.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

struct RecorderOutput {
	AudioOutput base;

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

	RecorderOutput()
		:base(recorder_output_plugin) {}

	bool Initialize(const config_param &param, Error &error_r) {
		return base.Configure(param, error_r);
	}

	bool Configure(const config_param &param, Error &error);

	bool WriteToFile(const void *data, size_t length, Error &error);

	/**
	 * Writes pending data from the encoder to the output file.
	 */
	bool EncoderToFile(Error &error);
};

static constexpr Domain recorder_output_domain("recorder_output");

inline bool
RecorderOutput::Configure(const config_param &param, Error &error)
{
	/* read configuration */

	const char *encoder_name =
		param.GetBlockValue("encoder", "vorbis");
	const auto encoder_plugin = encoder_plugin_get(encoder_name);
	if (encoder_plugin == nullptr) {
		error.Format(config_domain,
			     "No such encoder: %s", encoder_name);
		return false;
	}

	path = param.GetBlockValue("path");
	if (path == nullptr) {
		error.Set(config_domain, "'path' not configured");
		return false;
	}

	/* initialize encoder */

	encoder = encoder_init(*encoder_plugin, param, error);
	if (encoder == nullptr)
		return false;

	return true;
}

static AudioOutput *
recorder_output_init(const config_param &param, Error &error)
{
	RecorderOutput *recorder = new RecorderOutput();

	if (!recorder->Initialize(param, error)) {
		delete recorder;
		return nullptr;
	}

	if (!recorder->Configure(param, error)) {
		delete recorder;
		return nullptr;
	}

	return &recorder->base;
}

static void
recorder_output_finish(AudioOutput *ao)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	encoder_finish(recorder->encoder);
	delete recorder;
}

inline bool
RecorderOutput::WriteToFile(const void *_data, size_t length, Error &error)
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
			error.Set(recorder_output_domain,
				  "write() returned 0");
			return false;
		} else if (errno != EINTR) {
			error.FormatErrno("Failed to write to '%s'", path);
			return false;
		}
	}
}

inline bool
RecorderOutput::EncoderToFile(Error &error)
{
	assert(fd >= 0);

	while (true) {
		/* read from the encoder */

		size_t size = encoder_read(encoder, buffer, sizeof(buffer));
		if (size == 0)
			return true;

		/* write everything into the file */

		if (!WriteToFile(buffer, size, error))
			return false;
	}
}

static bool
recorder_output_open(AudioOutput *ao,
		     AudioFormat &audio_format,
		     Error &error)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	/* create the output file */

	recorder->fd = open_cloexec(recorder->path,
				    O_CREAT|O_WRONLY|O_TRUNC|O_BINARY,
				    0666);
	if (recorder->fd < 0) {
		error.FormatErrno("Failed to create '%s'", recorder->path);
		return false;
	}

	/* open the encoder */

	if (!encoder_open(recorder->encoder, audio_format, error)) {
		close(recorder->fd);
		unlink(recorder->path);
		return false;
	}

	if (!recorder->EncoderToFile(error)) {
		encoder_close(recorder->encoder);
		close(recorder->fd);
		unlink(recorder->path);
		return false;
	}

	return true;
}

static void
recorder_output_close(AudioOutput *ao)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	/* flush the encoder and write the rest to the file */

	if (encoder_end(recorder->encoder, IgnoreError()))
		recorder->EncoderToFile(IgnoreError());

	/* now really close everything */

	encoder_close(recorder->encoder);

	close(recorder->fd);
}

static size_t
recorder_output_play(AudioOutput *ao, const void *chunk, size_t size,
		     Error &error)
{
	RecorderOutput *recorder = (RecorderOutput *)ao;

	return encoder_write(recorder->encoder, chunk, size, error) &&
		recorder->EncoderToFile(error)
		? size : 0;
}

const struct AudioOutputPlugin recorder_output_plugin = {
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
