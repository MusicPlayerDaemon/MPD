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
#include "FifoOutputPlugin.hxx"
#include "config/ConfigError.hxx"
#include "../OutputAPI.hxx"
#include "../Timer.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "open.h"

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define FIFO_BUFFER_SIZE 65536 /* pipe capacity on Linux >= 2.6.11 */

struct FifoOutput {
	AudioOutput base;

	AllocatedPath path;
	std::string path_utf8;

	int input;
	int output;
	bool created;
	Timer *timer;

	FifoOutput()
		:base(fifo_output_plugin),
		 path(AllocatedPath::Null()), input(-1), output(-1),
		 created(false) {}

	bool Initialize(const config_param &param, Error &error) {
		return base.Configure(param, error);
	}

	bool Create(Error &error);
	bool Check(Error &error);
	void Delete();

	bool Open(Error &error);
	void Close();
};

static constexpr Domain fifo_output_domain("fifo_output");

inline void
FifoOutput::Delete()
{
	FormatDebug(fifo_output_domain,
		    "Removing FIFO \"%s\"", path_utf8.c_str());

	if (!RemoveFile(path)) {
		FormatErrno(fifo_output_domain,
			    "Could not remove FIFO \"%s\"",
			    path_utf8.c_str());
		return;
	}

	created = false;
}

void
FifoOutput::Close()
{
	if (input >= 0) {
		close(input);
		input = -1;
	}

	if (output >= 0) {
		close(output);
		output = -1;
	}

	struct stat st;
	if (created && StatFile(path, st))
		Delete();
}

inline bool
FifoOutput::Create(Error &error)
{
	if (!MakeFifo(path, 0666)) {
		error.FormatErrno("Couldn't create FIFO \"%s\"",
				  path_utf8.c_str());
		return false;
	}

	created = true;
	return true;
}

inline bool
FifoOutput::Check(Error &error)
{
	struct stat st;
	if (!StatFile(path, st)) {
		if (errno == ENOENT) {
			/* Path doesn't exist */
			return Create(error);
		}

		error.FormatErrno("Failed to stat FIFO \"%s\"",
				  path_utf8.c_str());
		return false;
	}

	if (!S_ISFIFO(st.st_mode)) {
		error.Format(fifo_output_domain,
			     "\"%s\" already exists, but is not a FIFO",
			     path_utf8.c_str());
		return false;
	}

	return true;
}

inline bool
FifoOutput::Open(Error &error)
{
	if (!Check(error))
		return false;

	input = OpenFile(path, O_RDONLY|O_NONBLOCK|O_BINARY, 0);
	if (input < 0) {
		error.FormatErrno("Could not open FIFO \"%s\" for reading",
				  path_utf8.c_str());
		Close();
		return false;
	}

	output = OpenFile(path, O_WRONLY|O_NONBLOCK|O_BINARY, 0);
	if (output < 0) {
		error.FormatErrno("Could not open FIFO \"%s\" for writing",
				  path_utf8.c_str());
		Close();
		return false;
	}

	return true;
}

static bool
fifo_open(FifoOutput *fd, Error &error)
{
	return fd->Open(error);
}

static AudioOutput *
fifo_output_init(const config_param &param, Error &error)
{
	FifoOutput *fd = new FifoOutput();

	fd->path = param.GetBlockPath("path", error);
	if (fd->path.IsNull()) {
		delete fd;

		if (!error.IsDefined())
			error.Set(config_domain,
				  "No \"path\" parameter specified");
		return nullptr;
	}

	fd->path_utf8 = fd->path.ToUTF8();

	if (!fd->Initialize(param, error)) {
		delete fd;
		return nullptr;
	}

	if (!fifo_open(fd, error)) {
		delete fd;
		return nullptr;
	}

	return &fd->base;
}

static void
fifo_output_finish(AudioOutput *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	fd->Close();
	delete fd;
}

static bool
fifo_output_open(AudioOutput *ao, AudioFormat &audio_format,
		 gcc_unused Error &error)
{
	FifoOutput *fd = (FifoOutput *)ao;

	fd->timer = new Timer(audio_format);

	return true;
}

static void
fifo_output_close(AudioOutput *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	delete fd->timer;
}

static void
fifo_output_cancel(AudioOutput *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;
	char buf[FIFO_BUFFER_SIZE];
	int bytes = 1;

	fd->timer->Reset();

	while (bytes > 0 && errno != EINTR)
		bytes = read(fd->input, buf, FIFO_BUFFER_SIZE);

	if (bytes < 0 && errno != EAGAIN) {
		FormatErrno(fifo_output_domain,
			    "Flush of FIFO \"%s\" failed",
			    fd->path_utf8.c_str());
	}
}

static unsigned
fifo_output_delay(AudioOutput *ao)
{
	FifoOutput *fd = (FifoOutput *)ao;

	return fd->timer->IsStarted()
		? fd->timer->GetDelay()
		: 0;
}

static size_t
fifo_output_play(AudioOutput *ao, const void *chunk, size_t size,
		 Error &error)
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

			error.FormatErrno("Failed to write to FIFO %s",
					  fd->path_utf8.c_str());
			return 0;
		}
	}
}

const struct AudioOutputPlugin fifo_output_plugin = {
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
