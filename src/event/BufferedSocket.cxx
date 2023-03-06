// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "BufferedSocket.hxx"
#include "net/SocketError.hxx"

#include <stdexcept>

BufferedSocket::ssize_t
BufferedSocket::DirectRead(void *data, size_t length) noexcept
{
	const auto nbytes = GetSocket().Read((char *)data, length);
	if (nbytes > 0) [[likely]]
		return nbytes;

	if (nbytes == 0) {
		OnSocketClosed();
		return -1;
	}

	const auto code = GetSocketError();
	if (IsSocketErrorReceiveWouldBlock(code))
		return 0;

	if (IsSocketErrorClosed(code))
		OnSocketClosed();
	else
		OnSocketError(std::make_exception_ptr(MakeSocketError(code, "Failed to receive from socket")));
	return -1;
}

bool
BufferedSocket::ReadToBuffer() noexcept
{
	assert(IsDefined());

	const auto buffer = input.Write();
	assert(!buffer.empty());

	const auto nbytes = DirectRead(buffer.data(), buffer.size());
	if (nbytes > 0)
		input.Append(nbytes);

	return nbytes >= 0;
}

bool
BufferedSocket::ResumeInput() noexcept
{
	assert(IsDefined());

	while (true) {
		const auto buffer = input.Read();
		if (buffer.empty()) {
			event.ScheduleRead();
			return true;
		}

		const auto result = OnSocketInput(buffer.data(), buffer.size());
		switch (result) {
		case InputResult::MORE:
			if (input.IsFull()) {
				OnSocketError(std::make_exception_ptr(std::runtime_error("Input buffer is full")));
				return false;
			}

			event.ScheduleRead();
			return true;

		case InputResult::PAUSE:
			event.CancelRead();
			return true;

		case InputResult::AGAIN:
			continue;

		case InputResult::CLOSED:
			return false;
		}
	}
}

void
BufferedSocket::OnSocketReady(unsigned flags) noexcept
{
	assert(IsDefined());

	if (flags & (SocketEvent::ERROR|SocketEvent::HANGUP)) [[unlikely]] {
		OnSocketClosed();
		return;
	}

	if (flags & SocketEvent::READ) {
		assert(!input.IsFull());

		if (!ReadToBuffer() || !ResumeInput())
			return;

		if (!input.IsFull())
			event.ScheduleRead();
	}
}
