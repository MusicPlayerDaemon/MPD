/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "../Wrapper.hxx"
#include "../Timer.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/FileInfo.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "open.h"

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

class FifoOutput {
	friend struct AudioOutputWrapper<FifoOutput>;

	AudioOutput base;

	AllocatedPath path;
	std::string path_utf8;

	int input;
	int output;
	bool created;
	Timer *timer;

public:
	FifoOutput()
		:base(fifo_output_plugin),
		 path(AllocatedPath::Null()), input(-1), output(-1),
		 created(false) {}

	~FifoOutput() {
		CloseFifo();
	}

	bool Initialize(const ConfigBlock &block, Error &error) {
		return base.Configure(block, error);
	}

	static FifoOutput *Create(const ConfigBlock &block, Error &error);

	bool Create(Error &error);
	bool Check(Error &error);
	void Delete();

	bool OpenFifo(Error &error);
	void CloseFifo();

	bool Open(AudioFormat &audio_format, Error &error);
	void Close();

	unsigned Delay() const;
	size_t Play(const void *chunk, size_t size, Error &error);
	void Cancel();
};

static constexpr Domain fifo_output_domain("fifo_output");

inline void
FifoOutput::Delete()
{
	FormatDebug(fifo_output_domain,
		    "Removing FIFO \"%s\"", path_utf8.c_str());

	try {
		RemoveFile(path);
	} catch (const std::runtime_error &e) {
		LogError(e, "Could not remove FIFO");
		return;
	}

	created = false;
}

void
FifoOutput::CloseFifo()
{
	if (input >= 0) {
		close(input);
		input = -1;
	}

	if (output >= 0) {
		close(output);
		output = -1;
	}

	FileInfo fi;
	if (created && GetFileInfo(path, fi))
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
FifoOutput::OpenFifo(Error &error)
{
	if (!Check(error))
		return false;

	input = OpenFile(path, O_RDONLY|O_NONBLOCK|O_BINARY, 0);
	if (input < 0) {
		error.FormatErrno("Could not open FIFO \"%s\" for reading",
				  path_utf8.c_str());
		CloseFifo();
		return false;
	}

	output = OpenFile(path, O_WRONLY|O_NONBLOCK|O_BINARY, 0);
	if (output < 0) {
		error.FormatErrno("Could not open FIFO \"%s\" for writing",
				  path_utf8.c_str());
		CloseFifo();
		return false;
	}

	return true;
}

inline FifoOutput *
FifoOutput::Create(const ConfigBlock &block, Error &error)
{
	FifoOutput *fd = new FifoOutput();

	fd->path = block.GetPath("path");
	if (fd->path.IsNull()) {
		delete fd;
		throw std::runtime_error("No \"path\" parameter specified");
	}

	fd->path_utf8 = fd->path.ToUTF8();

	if (!fd->Initialize(block, error)) {
		delete fd;
		return nullptr;
	}

	if (!fd->OpenFifo(error)) {
		delete fd;
		return nullptr;
	}

	return fd;
}

bool
FifoOutput::Open(AudioFormat &audio_format, gcc_unused Error &error)
{
	timer = new Timer(audio_format);
	return true;
}

void
FifoOutput::Close()
{
	delete timer;
}

inline void
FifoOutput::Cancel()
{
	timer->Reset();

	ssize_t bytes;
	do {
		char buffer[16384];
		bytes = read(input, buffer, sizeof(buffer));
	} while (bytes > 0 && errno != EINTR);

	if (bytes < 0 && errno != EAGAIN) {
		FormatErrno(fifo_output_domain,
			    "Flush of FIFO \"%s\" failed",
			    path_utf8.c_str());
	}
}

inline unsigned
FifoOutput::Delay() const
{
	return timer->IsStarted()
		? timer->GetDelay()
		: 0;
}

inline size_t
FifoOutput::Play(const void *chunk, size_t size, Error &error)
{
	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	while (true) {
		ssize_t bytes = write(output, chunk, size);
		if (bytes > 0)
			return (size_t)bytes;

		if (bytes < 0) {
			switch (errno) {
			case EAGAIN:
				/* The pipe is full, so empty it */
				Cancel();
				continue;
			case EINTR:
				continue;
			}

			error.FormatErrno("Failed to write to FIFO %s",
					  path_utf8.c_str());
			return 0;
		}
	}
}

typedef AudioOutputWrapper<FifoOutput> Wrapper;

const struct AudioOutputPlugin fifo_output_plugin = {
	"fifo",
	nullptr,
	&Wrapper::Init,
	&Wrapper::Finish,
	nullptr,
	nullptr,
	&Wrapper::Open,
	&Wrapper::Close,
	&Wrapper::Delay,
	nullptr,
	&Wrapper::Play,
	nullptr,
	&Wrapper::Cancel,
	nullptr,
	nullptr,
};
