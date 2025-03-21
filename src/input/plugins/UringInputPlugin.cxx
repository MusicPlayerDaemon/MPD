// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UringInputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "event/Call.hxx"
#include "event/Loop.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/uring/ReadOperation.hxx"
#include "io/uring/Queue.hxx"

#include <sys/stat.h>

/**
 * Read at most this number of bytes in each read request.
 */
static const size_t URING_MAX_READ = 256 * 1024;

/**
 * Do not buffer more than this number of bytes.  It should be a
 * reasonable limit that doesn't make low-end machines suffer too
 * much, but doesn't cause stuttering on high-latency lines.
 */
static const size_t URING_MAX_BUFFERED = 512 * 1024;

/**
 * Resume the stream at this number of bytes after it has been paused.
 */
static const size_t URING_RESUME_AT = 384 * 1024;

static EventLoop *uring_input_event_loop;
static Uring::Queue *uring_input_queue;
static bool uring_input_initialized = false;

class UringInputStream final : public AsyncInputStream, Uring::ReadHandler {
	Uring::Queue &uring;

	UniqueFileDescriptor fd;

	uint64_t next_offset = 0;

	std::unique_ptr<Uring::ReadOperation> read_operation;

public:
	UringInputStream(EventLoop &event_loop, Uring::Queue &_uring,
			 std::string_view path,
			 UniqueFileDescriptor &&_fd,
			 offset_type _size, Mutex &_mutex)
		:AsyncInputStream(event_loop,
				  path, _mutex,
				  URING_MAX_BUFFERED,
				  URING_RESUME_AT),
		 uring(_uring),
		 fd(std::move(_fd))
	{
		size = _size;
		seekable = true;
		SetReady();

		BlockingCall(GetEventLoop(), [this](){
			SubmitRead();
		});
	}

	~UringInputStream() noexcept override {
		BlockingCall(GetEventLoop(), [this](){
			CancelRead();
		});
	}

private:
	void SubmitRead() noexcept;

	void CancelRead() noexcept {
		if (read_operation)
			read_operation.release()->Cancel();
	}

protected:
	/* virtual methods from AsyncInputStream */
	void DoResume() override;
	void DoSeek(offset_type new_offset) override;

private:
	/* virtual methods from class Uring::Operation */
	void OnRead(std::unique_ptr<std::byte[]> buffer,
		    std::size_t size) noexcept override;
	void OnReadError(int error) noexcept override;
};

void
UringInputStream::SubmitRead() noexcept
{
	assert(!read_operation);

	int64_t remaining = size - next_offset;
	if (remaining <= 0)
		return;

	auto w = PrepareWriteBuffer();
	if (w.empty()) {
		Pause();
		return;
	}

	read_operation = std::make_unique<Uring::ReadOperation>();
	read_operation->Start(uring, fd, next_offset,
			      std::min(w.size(), URING_MAX_READ),
			      *this);
}

void
UringInputStream::DoResume()
{
	SubmitRead();
}

void
UringInputStream::DoSeek(offset_type new_offset)
{
	CancelRead();

	next_offset = offset = new_offset;
	SeekDone();
	SubmitRead();
}

void
UringInputStream::OnRead(std::unique_ptr<std::byte[]> data,
			 std::size_t nbytes) noexcept
{
	read_operation.reset();

	const std::scoped_lock protect{mutex};

	if (nbytes == 0) {
		postponed_exception = std::make_exception_ptr(std::runtime_error("Premature end of file"));
		InvokeOnAvailable();
		return;
	}

	auto w = PrepareWriteBuffer();
	assert(w.size() >= nbytes);
	memcpy(w.data(), data.get(), nbytes);
	CommitWriteBuffer(nbytes);
	next_offset += nbytes;
	SubmitRead();
}

void
UringInputStream::OnReadError(int error) noexcept
{
	read_operation.reset();

	const std::scoped_lock protect{mutex};

	postponed_exception = std::make_exception_ptr(MakeErrno(error, "Read failed"));
	InvokeOnAvailable();
}

InputStreamPtr
OpenUringInputStream(const char *path, Mutex &mutex)
{
	if (!uring_input_initialized) {
		BlockingCall(*uring_input_event_loop, [](){
			if (uring_input_initialized)
				return;

			uring_input_queue = uring_input_event_loop->GetUring();
			uring_input_initialized = true;
		});
	}

	if (uring_input_queue == nullptr)
		return nullptr;

	// TODO: use IORING_OP_OPENAT
	auto fd = OpenReadOnly(path);

	// TODO: use IORING_OP_STATX
	struct stat st;
	if (fstat(fd.Get(), &st) < 0)
		throw FmtErrno("Failed to access {}", path);

	if (!S_ISREG(st.st_mode))
		throw FmtRuntimeError("Not a regular file: {}", path);

	return std::make_unique<UringInputStream>(*uring_input_event_loop,
						  *uring_input_queue,
						  path, std::move(fd),
						  st.st_size, mutex);
}

void
InitUringInputPlugin(EventLoop &event_loop) noexcept
{
	uring_input_event_loop = &event_loop;
}
