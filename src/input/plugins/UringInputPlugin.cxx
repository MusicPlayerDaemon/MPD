/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "UringInputPlugin.hxx"
#include "../AsyncInputStream.hxx"
#include "event/Call.hxx"
#include "event/Loop.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/uring/Operation.hxx"
#include "io/uring/Queue.hxx"
#include "util/RuntimeError.hxx"

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

class UringInputStream final : public AsyncInputStream, Uring::Operation {
	Uring::Queue &uring;

	UniqueFileDescriptor fd;

	uint64_t next_offset = 0;

	struct iovec iov;

public:
	UringInputStream(EventLoop &event_loop, Uring::Queue &_uring,
			 const char *path,
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

	~UringInputStream() override {
		BlockingCall(GetEventLoop(), [this](){
			CancelUring();
		});
	}

private:
	void SubmitRead() noexcept;

protected:
	/* virtual methods from AsyncInputStream */
	void DoResume() override;
	void DoSeek(offset_type new_offset) override;

private:
	/* virtual methods from class Uring::Operation */
	void OnUringCompletion(int res) noexcept override;
};

void
UringInputStream::SubmitRead() noexcept
{
	assert(!IsUringPending());

	int64_t remaining = size - next_offset;
	if (remaining <= 0)
		return;

	auto w = PrepareWriteBuffer();
	if (w.empty()) {
		Pause();
		return;
	}

	auto *s = uring.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	iov.iov_base = w.data;
	iov.iov_len = std::min<size_t>(std::min<uint64_t>(remaining,
							  URING_MAX_READ),
				       w.size);

	io_uring_prep_readv(s, fd.Get(), &iov, 1, next_offset);
	uring.Push(*s, *this);
}

void
UringInputStream::DoResume()
{
	SubmitRead();
}

void
UringInputStream::DoSeek(offset_type new_offset)
{
	CancelUring();

	next_offset = offset = new_offset;
	SeekDone();
	SubmitRead();
}

void
UringInputStream::OnUringCompletion(int res) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	assert(!IsBufferFull());
	assert(IsBufferFull() == (GetBufferSpace() == 0));

	if (res <= 0) {
		try {
			if (res == 0)
				throw std::runtime_error("Premature end of file");
			else
				throw MakeErrno(-res, "Read failed");
		} catch (...) {
			postponed_exception = std::current_exception();
		}

		InvokeOnAvailable();
		return;
	}

	CommitWriteBuffer(res);
	next_offset += res;
	SubmitRead();
}

InputStreamPtr
OpenUringInputStream(const char *path, Mutex &mutex)
{
	if (uring_input_queue == nullptr)
		return nullptr;

	// TODO: use IORING_OP_OPENAT
	auto fd = OpenReadOnly(path);

	// TODO: use IORING_OP_STATX
	struct stat st;
	if (fstat(fd.Get(), &st) < 0)
		throw FormatErrno("Failed to access %s", path);

	if (!S_ISREG(st.st_mode))
		throw FormatRuntimeError("Not a regular file: %s", path);

	return std::make_unique<UringInputStream>(*uring_input_event_loop,
						  *uring_input_queue,
						  path, std::move(fd),
						  st.st_size, mutex);
}

void
InitUringInputPlugin(EventLoop &event_loop) noexcept
{
	uring_input_event_loop = &event_loop;

	BlockingCall(event_loop, [](){
		uring_input_queue = uring_input_event_loop->GetUring();
	});
}
