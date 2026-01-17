// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "SocketEvent.hxx"
#include "io/FileDescriptor.hxx"

/**
 * A variant of #SocketEvent for pipes (and other kinds of
 * #FileDescriptor which can be used with epoll).
 */
class PipeEvent final
	: public EventPollBackendEvents
{
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

	unsigned GetScheduledFlags() const noexcept {
		return event.GetScheduledFlags();
	}

	unsigned GetReadyFlags() const noexcept {
		return event.GetReadyFlags();
	}

	void SetReadyFlags(unsigned flags) noexcept {
		event.SetReadyFlags(flags);
	}

	void ClearReadyFlags(unsigned flags) noexcept {
		event.ClearReadyFlags(flags);
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

	void CancelOnlyRead() noexcept {
		event.CancelOnlyRead();
	}

	void CancelOnlyWrite() noexcept {
		event.CancelOnlyWrite();
	}

	void ScheduleImplicit() noexcept {
		event.ScheduleImplicit();
	}

	void ScheduleAnyHangup() noexcept {
		event.ScheduleAnyHangup();
	}

	bool IsReadPending() const noexcept {
		return event.IsReadPending();
	}

	bool IsWritePending() const noexcept {
		return event.IsWritePending();
	}
};
