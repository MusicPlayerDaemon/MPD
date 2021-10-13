/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SocketEvent.hxx"
#include "io/FileDescriptor.hxx"

/**
 * A variant of #SocketEvent for pipes (and other kinds of
 * #FileDescriptor which can be used with epoll).
 */
class PipeEvent final {
	SocketEvent event;

public:
	template<typename C>
	PipeEvent(EventLoop &event_loop, C callback,
		  FileDescriptor fd=FileDescriptor::Undefined()) noexcept
		:event(event_loop, callback,
		       SocketDescriptor::FromFileDescriptor(fd)) {}

	EventLoop &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	FileDescriptor GetFileDescriptor() const noexcept {
		return event.GetSocket().ToFileDescriptor();
	}

	FileDescriptor ReleaseFileDescriptor() noexcept {
		return event.ReleaseSocket().ToFileDescriptor();
	}

	void Open(FileDescriptor fd) noexcept {
		event.Open(SocketDescriptor::FromFileDescriptor(fd));
	}

	void Close() noexcept {
		event.Close();
	}

	bool Schedule(unsigned flags) noexcept {
		return event.Schedule(flags);
	}

	void Cancel() noexcept {
		event.Cancel();
	}

	bool ScheduleRead() noexcept {
		return event.ScheduleRead();
	}

	bool ScheduleWrite() noexcept {
		return event.ScheduleWrite();
	}

	void CancelRead() noexcept {
		event.CancelRead();
	}

	void CancelWrite() noexcept {
		event.CancelWrite();
	}

	void ScheduleImplicit() noexcept {
		event.ScheduleImplicit();
	}
};
