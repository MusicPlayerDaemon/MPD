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
#include "FifoOutputPlugin.hxx"
#include "OutputAPI.hxx"
#include "Timer.hxx"
#include "fd_util.h"
#include "open.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "fifo"

#define FIFO_BUFFER_SIZE 65536 /* pipe capacity on Linux >= 2.6.11 */

struct FifoOutput {
	struct audio_output base;

	char *path;
	int input;
	int output;
	bool created;
	Timer *timer;

	FifoOutput()
		:path(nullptr), input(-1), output(-1), created(false) {}

	~FifoOutput() {
		g_free(path);
	}

	bool Initialize(const config_param &param, GError **error_r) {
		return ao_base_init(&base, &fifo_output_plugin, param,
				    error_r);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}

	bool Create(GError **error_r);
	bool Check(GError **error_r);
	void Delete();

	bool Open(GError **error_r);
	void Close();
};

/**
 * The quark used for GError.domain.
 */
static inline GQuark
fifo_output_quark(void)
{
	return g_quark_from_static_string("fifo_output");
}

inline void
FifoOutput::Delete()
{
	g_debug("Removing FIFO \"%s\"", path);

	if (unlink(path) < 0) {
		g_warning("Could not remove FIFO \"%s\": %s",
			  path, g_strerror(errno));
		return;
	}

	created = false;
}

void
FifoOutput::Close()
{
	struct stat st;

	if (input >= 0) {
		close(input);
		input = -1;
	}

	if (output >= 0) {
		close(output);
		output = -1;
	}

	if (created && (stat(path, &st) == 0))
		Delete();
}

inline bool
FifoOutput::Create(GError **error_r)
{
	if (mkfifo(path, 0666) < 0) {
		g_set_error(error_r, fifo_output_quark(), errno,
			    "Couldn't create FIFO \"%s\": %s",
			    path, g_strerror(errno));
		return false;
	}

	created = true;
	return true;
}

inline bool
FifoOutput::Check(GError **error_r)
{
	struct stat st;
	if (stat(path, &st) < 0) {
		if (errno == ENOENT) {
			/* Path doesn't exist */
			return Create(error_r);
		}

		g_set_error(error_r, fifo_output_quark(), errno,
			    "Failed to stat FIFO \"%s\": %s",
			    path, g_strerror(errno));
		return false;
	}

	if (!S_ISFIFO(st.st_mode)) {
		g_set_error(error_r, fifo_output_quark(), 0,
			    "\"%s\" already exists, but is not a FIFO",
			    path);
		return false;
	}

	return true;
}

inline bool
FifoOutput::Open(GError **error_r)
{
	if (!Check(error_r))
		return false;

	input = open_cloexec(path, O_RDONLY|O_NONBLOCK|O_BINARY, 0);
	if (input < 0) {
		g_set_error(error_r, fifo_output_quark(), errno,
			    "Could not open FIFO \"%s\" for reading: %s",
			    path, g_strerror(errno));
		Close();
		return false;
	}

	output = open_cloexec(path, O_WRONLY|O_NONBLOCK|O_BINARY, 0);
	if (output < 0) {
		g_set_error(error_r, fifo_output_quark(), errno,
			    "Could not open FIFO \"%s\" for writing: %s",
			    path, g_strerror(errno));
		Close();
		return false;
	}

	return true;
}

static bool
fifo_open(FifoOutput *fd, GError **error_r)
{
	return fd->Open(error_r);
}

static struct audio_output *
fifo_output_init(const config_param &param, GError **error_r)
{
	GError *error = nullptr;
	char *path = param.DupBlockPath("path", &error);
	if (!path) {
		if (error != nullptr)
			g_propagate_error(error_r, error);
		else
			g_set_error(error_r, fifo_output_quark(), 0,
				    "No \"path\" parameter specified");
		return nullptr;
	}

	FifoOutput *fd = new FifoOutput();
	fd->path = path;

	if (!fd->Initialize(param, error_r)) {
		delete fd;
		return nullptr;
	}

	if (!fifo_open(fd, error_r)) {
		fd->Deinitialize();
		delete fd;
		return nullptr;
	}

	return &fd->base;
}

static void
fifo_output_finish(struct audio_output *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	fd->Close();
	fd->Deinitialize();
	delete fd;
}

static bool
fifo_output_open(struct audio_output *ao, AudioFormat &audio_format,
		 gcc_unused GError **error)
{
	FifoOutput *fd = (FifoOutput *)ao;

	fd->timer = new Timer(audio_format);

	return true;
}

static void
fifo_output_close(struct audio_output *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	delete fd->timer;
}

static void
fifo_output_cancel(struct audio_output *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;
	char buf[FIFO_BUFFER_SIZE];
	int bytes = 1;

	fd->timer->Reset();

	while (bytes > 0 && errno != EINTR)
		bytes = read(fd->input, buf, FIFO_BUFFER_SIZE);

	if (bytes < 0 && errno != EAGAIN) {
		g_warning("Flush of FIFO \"%s\" failed: %s",
			  fd->path, g_strerror(errno));
	}
}

static unsigned
fifo_output_delay(struct audio_output *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	return fd->timer->IsStarted()
		? fd->timer->GetDelay()
		: 0;
}

static size_t
fifo_output_play(struct audio_output *ao, const void *chunk, size_t size,
		 GError **error)
{
	FifoOutput *fd = (FifoOutput *)ao;
	ssize_t bytes;

	if (!fd->timer->IsStarted())
		fd->timer->Start();
	fd->timer->Add(size);

	while (true) {
		bytes = write(fd->output, chunk, size);
		if (bytes > 0)
			return (size_t)bytes;

		if (bytes < 0) {
			switch (errno) {
			case EAGAIN:
				/* The pipe is full, so empty it */
				fifo_output_cancel(&fd->base);
				continue;
			case EINTR:
				continue;
			}

			g_set_error(error, fifo_output_quark(), errno,
				    "Failed to write to FIFO %s: %s",
				    fd->path, g_strerror(errno));
			return 0;
		}
	}
}

const struct audio_output_plugin fifo_output_plugin = {
	"fifo",
	nullptr,
	fifo_output_init,
	fifo_output_finish,
	nullptr,
	nullptr,
	fifo_output_open,
	fifo_output_close,
	fifo_output_delay,
	nullptr,
	fifo_output_play,
	nullptr,
	fifo_output_cancel,
	nullptr,
	nullptr,
};
