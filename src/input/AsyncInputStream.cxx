// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AsyncInputStream.hxx"
#include "tag/Tag.hxx"
#include "event/Loop.hxx"

#include <cassert>
#include <stdexcept>

#include <string.h>

AsyncInputStream::AsyncInputStream(EventLoop &event_loop, std::string_view _url,
				   Mutex &_mutex,
				   size_t _buffer_size,
				   size_t _resume_at) noexcept
	:InputStream(_url, _mutex),
	 deferred_resume(event_loop, BIND_THIS_METHOD(DeferredResume)),
	 deferred_seek(event_loop, BIND_THIS_METHOD(DeferredSeek)),
	 allocation(_buffer_size),
	 resume_at(_resume_at)
{
	allocation.SetName("InputStream");
	allocation.ForkCow(false);
}

AsyncInputStream::~AsyncInputStream() noexcept
{
	buffer.Clear();
}

void
AsyncInputStream::SetTag(std::unique_ptr<Tag> _tag) noexcept
{
	tag = std::move(_tag);
}

void
AsyncInputStream::ClearTag() noexcept
{
	tag.reset();
}

void
AsyncInputStream::Pause() noexcept
{
	assert(GetEventLoop().IsInside());

	paused = true;
}

inline void
AsyncInputStream::Resume()
{
	assert(GetEventLoop().IsInside());

	if (paused) {
		paused = false;

		DoResume();
	}
}

void
AsyncInputStream::Check()
{
	if (postponed_exception)
		std::rethrow_exception(std::exchange(postponed_exception,
						     std::exception_ptr()));
}

bool
AsyncInputStream::IsEOF() const noexcept
{
	return (KnownSize() && offset >= size) ||
		(!open && buffer.empty());
}

void
AsyncInputStream::Seek(std::unique_lock<Mutex> &lock,
		       offset_type new_offset)
{
	assert(IsReady());
	assert(seek_state == SeekState::NONE);

	if (new_offset == offset) {
		/* no-op, but if the stream is not open anymore (maybe
		   because it has failed), nothing can be read, so we
		   should check for errors here instead of pretending
		   everything's fine */

		if (!open)
			Check();

		return;
	}

	if (!IsSeekable())
		throw std::runtime_error("Not seekable");

	/* check if we can fast-forward the buffer */

	while (new_offset > offset) {
		auto r = buffer.Read();
		if (r.empty())
			break;

		const size_t nbytes =
			std::cmp_less(new_offset - offset, r.size())
			? new_offset - offset
			: r.size();

		buffer.Consume(nbytes);
		offset += nbytes;
	}

	if (new_offset == offset)
		return;

	/* no: ask the implementation to seek */

	seek_offset = new_offset;
	seek_state = SeekState::SCHEDULED;

	deferred_seek.Schedule();

	caller_cond.wait(lock, [this]{ return seek_state == SeekState::NONE; });

	Check();
}

void
AsyncInputStream::SeekDone() noexcept
{
	assert(GetEventLoop().IsInside());
	assert(IsSeekPending());

	/* we may have reached end-of-file previously, and the
	   connection may have been closed already; however after
	   seeking successfully, the connection must be alive again */
	open = true;

	seek_state = SeekState::NONE;
	caller_cond.notify_one();
	InvokeOnAvailable();
}

std::unique_ptr<Tag>
AsyncInputStream::ReadTag() noexcept
{
	return std::exchange(tag, nullptr);
}

bool
AsyncInputStream::IsAvailable() const noexcept
{
	return postponed_exception ||
		IsEOF() ||
		!buffer.empty();
}

inline std::size_t
AsyncInputStream::ReadFromBuffer(std::span<std::byte> dest) noexcept
{
	const size_t nbytes = buffer.MoveTo(dest);
	if (nbytes == 0)
		return 0;

	if (buffer.empty())
		/* when the buffer becomes empty, reset its head and
		   tail so the next write can fill the whole buffer
		   and not just the part after the tail */
		buffer.Clear();

	offset += (offset_type)nbytes;
	return nbytes;
}

size_t
AsyncInputStream::Read(std::unique_lock<Mutex> &lock,
		       std::span<std::byte> dest)
{
	assert(!GetEventLoop().IsInside());

	/* wait for data */
	while (true) {
		Check();

		if (std::size_t nbytes = ReadFromBuffer(dest); nbytes > 0) {
			if (paused && buffer.GetSize() < resume_at)
				deferred_resume.Schedule();

			return nbytes;
		}

		if (IsEOF())
			return 0;

		caller_cond.wait(lock);
	}
}

void
AsyncInputStream::CommitWriteBuffer(size_t nbytes) noexcept
{
	buffer.Append(nbytes);

	if (!IsReady())
		SetReady();
	else {
		caller_cond.notify_one();
		InvokeOnAvailable();
	}
}

void
AsyncInputStream::AppendToBuffer(std::span<const std::byte> src) noexcept
{
	auto w = buffer.Write();
	assert(!w.empty());

	std::span<const std::byte> second{};

	if (w.size() < src.size()) {
		second = src.subspan(w.size());
		src = src.first(w.size());
	}

	std::copy(src.begin(), src.end(), w.begin());
	buffer.Append(src.size());

	if (!second.empty()) {
		w = buffer.Write();
		assert(!w.empty());
		assert(w.size() >= second.size());

		std::copy(second.begin(), second.end(), w.begin());
		buffer.Append(second.size());
	}

	if (!IsReady())
		SetReady();
	else {
		caller_cond.notify_one();
		InvokeOnAvailable();
	}
}

void
AsyncInputStream::DeferredResume() noexcept
{
	const std::scoped_lock protect{mutex};

	if (postponed_exception) {
		/* do not proceed, first the caller must handle the
                   pending error */
		caller_cond.notify_one();
		InvokeOnAvailable();
		return;
	}

	try {
		Resume();
	} catch (...) {
		postponed_exception = std::current_exception();
		caller_cond.notify_one();
		InvokeOnAvailable();
	}
}

void
AsyncInputStream::DeferredSeek() noexcept
{
	const std::scoped_lock protect{mutex};
	if (seek_state != SeekState::SCHEDULED)
		return;

	if (postponed_exception) {
		/* do not proceed, first the caller must handle the
                   pending error */
		caller_cond.notify_one();
		InvokeOnAvailable();
		return;
	}

	try {
		Resume();

		seek_state = SeekState::PENDING;
		buffer.Clear();
		paused = false;

		DoSeek(seek_offset);
	} catch (...) {
		seek_state = SeekState::NONE;
		postponed_exception = std::current_exception();
		caller_cond.notify_one();
		InvokeOnAvailable();
	}
}
