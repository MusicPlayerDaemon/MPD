// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FullyBufferedSocket.hxx"
#include "net/SocketError.hxx"

#include <cassert>

#include <string.h>

FullyBufferedSocket::ssize_t
FullyBufferedSocket::DirectWrite(const void *data, size_t length) noexcept
{
	const auto nbytes = GetSocket().Write((const char *)data, length);
	if (nbytes < 0) [[unlikely]] {
		const auto code = GetSocketError();
		if (IsSocketErrorSendWouldBlock(code))
			return 0;

		idle_event.Cancel();
		event.Cancel();

		if (IsSocketErrorClosed(code))
			OnSocketClosed();
		else
			OnSocketError(std::make_exception_ptr(MakeSocketError(code, "Failed to send to socket")));
	}

	return nbytes;
}

bool
FullyBufferedSocket::Flush() noexcept
{
	assert(IsDefined());

	const auto data = output.Read();
	if (data.empty()) {
		idle_event.Cancel();
		event.CancelWrite();
		return true;
	}

	auto nbytes = DirectWrite(data.data(), data.size());
	if (nbytes <= 0) [[unlikely]]
		return nbytes == 0;

	output.Consume(nbytes);

	if (output.empty()) {
		idle_event.Cancel();
		event.CancelWrite();
	}

	return true;
}

bool
FullyBufferedSocket::Write(const void *data, size_t length) noexcept
{
	assert(IsDefined());

	if (length == 0)
		return true;

	const bool was_empty = output.empty();

	if (!output.Append({(const std::byte *)data, length})) {
		OnSocketError(std::make_exception_ptr(std::runtime_error("Output buffer is full")));
		return false;
	}

	if (was_empty)
		idle_event.Schedule();
	return true;
}

void
FullyBufferedSocket::OnSocketReady(unsigned flags) noexcept
{
	if (flags & SocketEvent::WRITE) {
		assert(!output.empty());
		assert(!idle_event.IsPending());

		if (!Flush())
			return;
	}

	BufferedSocket::OnSocketReady(flags);
}

void
FullyBufferedSocket::OnIdle() noexcept
{
	if (Flush() && !output.empty())
		event.ScheduleWrite();
}
