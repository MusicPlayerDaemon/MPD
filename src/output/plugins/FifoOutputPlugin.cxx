// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FifoOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Timer.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/FileInfo.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "open.h"

#include <cerrno>

#include <sys/stat.h>
#include <unistd.h>

class FifoOutput final : AudioOutput {
	const AllocatedPath path;

	int input = -1;
	int output = -1;
	bool created = false;
	Timer *timer;

public:
	explicit FifoOutput(const ConfigBlock &block);

	~FifoOutput() override {
		CloseFifo();
	}

	FifoOutput(const FifoOutput &) = delete;
	FifoOutput &operator=(const FifoOutput &) = delete;

	static AudioOutput *Create(EventLoop &,
				   const ConfigBlock &block) {
		return new FifoOutput(block);
	}

private:
	void Create();
	void Check();
	void Delete();

	void OpenFifo();
	void CloseFifo();

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	[[nodiscard]] std::chrono::steady_clock::duration Delay() const noexcept override;
	std::size_t Play(std::span<const std::byte> src) override;
	void Cancel() noexcept override;
};

static constexpr Domain fifo_output_domain("fifo_output");

FifoOutput::FifoOutput(const ConfigBlock &block)
	:AudioOutput(0),
	 path(block.GetPath("path"))
{
	if (path.IsNull())
		throw std::runtime_error("No \"path\" parameter specified");

	OpenFifo();
}

inline void
FifoOutput::Delete()
{
	FmtDebug(fifo_output_domain,
		 "Removing FIFO {:?}", path);

	try {
		RemoveFile(path);
	} catch (...) {
		LogError(std::current_exception(), "Could not remove FIFO");
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

inline void
FifoOutput::Create()
{
	if (!MakeFifo(path, 0666))
		throw FmtErrno("Couldn't create FIFO {:?}", path);

	created = true;
}

inline void
FifoOutput::Check()
{
	struct stat st;
	if (!StatFile(path, st)) {
		if (errno == ENOENT) {
			/* Path doesn't exist */
			Create();
			return;
		}

		throw FmtErrno("Failed to stat FIFO {:?}", path);
	}

	if (!S_ISFIFO(st.st_mode))
		throw FmtRuntimeError("{:?} already exists, but is not a FIFO",
				      path);
}

inline void
FifoOutput::OpenFifo()
try {
	Check();

	input = OpenFile(path, O_RDONLY|O_NONBLOCK|O_BINARY, 0).Steal();
	if (input < 0)
		throw FmtErrno("Could not open FIFO {:?} for reading",
			       path);

	output = OpenFile(path, O_WRONLY|O_NONBLOCK|O_BINARY, 0).Steal();
	if (output < 0)
		throw FmtErrno("Could not open FIFO {:?} for writing");
} catch (...) {
	CloseFifo();
	throw;
}

void
FifoOutput::Open(AudioFormat &audio_format)
{
	timer = new Timer(audio_format);
}

void
FifoOutput::Close() noexcept
{
	delete timer;
}

void
FifoOutput::Cancel() noexcept
{
	timer->Reset();

	ssize_t bytes;
	do {
		char buffer[16384];
		bytes = read(input, buffer, sizeof(buffer));
	} while (bytes > 0 && errno != EINTR);

	if (bytes < 0 && errno != EAGAIN) {
		FmtError(fifo_output_domain,
			 "Flush of FIFO {:?} failed: {}",
			 path, strerror(errno));
	}
}

std::chrono::steady_clock::duration
FifoOutput::Delay() const noexcept
{
	return timer->IsStarted()
		? timer->GetDelay()
		: std::chrono::steady_clock::duration::zero();
}

std::size_t
FifoOutput::Play(std::span<const std::byte> src)
{
	if (!timer->IsStarted())
		timer->Start();
	timer->Add(src.size());

	while (true) {
		ssize_t bytes = write(output, src.data(), src.size());
		if (bytes > 0)
			return (std::size_t)bytes;

		if (bytes < 0) {
			switch (errno) {
			case EAGAIN:
				/* The pipe is full, so empty it */
				Cancel();
				continue;
			case EINTR:
				continue;
			}

			throw FmtErrno("Failed to write to FIFO {}", path);
		}
	}
}

const struct AudioOutputPlugin fifo_output_plugin = {
	"fifo",
	nullptr,
	&FifoOutput::Create,
	nullptr,
};
