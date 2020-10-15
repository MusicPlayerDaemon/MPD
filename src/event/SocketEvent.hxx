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

#ifndef MPD_SOCKET_EVENT_HXX
#define MPD_SOCKET_EVENT_HXX

#include "PollGroup.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/BindMethod.hxx"

#include <boost/intrusive/list_hook.hpp>

#include <cassert>
#include <cstddef>
#include <type_traits>

class EventLoop;

/**
 * Monitor events on a socket.  Call Schedule() to announce events
 * you're interested in, or Cancel() to cancel your subscription.  The
 * #EventLoop will invoke the callback as soon as any of the
 * subscribed events are ready.
 *
 * This class does not feel responsible for closing the socket.  Call
 * Close() to do it manually.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class SocketEvent {
	friend class EventLoop;

	EventLoop &loop;

	using ReadyListHook = boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>;
	ReadyListHook ready_siblings;

	using Callback = BoundMethod<void(unsigned events) noexcept>;
	const Callback callback;

	SocketDescriptor fd;

	/**
	 * A bit mask of events that are currently registered in the
	 * #EventLoop.
	 */
	unsigned scheduled_flags = 0;

	/**
	 * A bit mask of events which have been reported as "ready" by
	 * epoll_wait().  If non-zero, then the #EventLoop will call
	 * Dispatch() soon.
	 */
	unsigned ready_flags = 0;

public:
	static constexpr unsigned READ = PollGroup::READ;
	static constexpr unsigned WRITE = PollGroup::WRITE;
	static constexpr unsigned ERROR = PollGroup::ERROR;
	static constexpr unsigned HANGUP = PollGroup::HANGUP;

	/**
	 * These flags are always reported by epoll_wait() and don't
	 * need to be registered with epoll_ctl().
	 */
	static constexpr unsigned IMPLICIT_FLAGS = ERROR|HANGUP;

	typedef std::make_signed<size_t>::type ssize_t;

	SocketEvent(EventLoop &_loop, Callback _callback,
		    SocketDescriptor _fd=SocketDescriptor::Undefined()) noexcept
		:loop(_loop),
		 callback(_callback),
		 fd(_fd) {}

	~SocketEvent() noexcept {
		Cancel();
	}

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsDefined() const noexcept {
		return fd.IsDefined();
	}

	SocketDescriptor GetSocket() const noexcept {
		assert(IsDefined());

		return fd;
	}

	SocketDescriptor ReleaseSocket() noexcept {
		Cancel();
		return std::exchange(fd, SocketDescriptor::Undefined());
	}

	void Open(SocketDescriptor _fd) noexcept;

	/**
	 * Close the socket (and cancel all scheduled events).
	 */
	void Close() noexcept;

	/**
	 * Call this instead of Cancel() to unregister this object
	 * after the underlying socket has already been closed.  This
	 * skips the `EPOLL_CTL_DEL` call because the kernel
	 * automatically removes closed file descriptors from epoll.
	 *
	 * Doing `EPOLL_CTL_DEL` on a closed file descriptor usually
	 * fails with `-EBADF` or could unregister a different socket
	 * which happens to be on the same file descriptor number.
	 */
	void Abandon() noexcept;

	unsigned GetScheduledFlags() const noexcept {
		return scheduled_flags;
	}

	void SetReadyFlags(unsigned flags) noexcept {
		ready_flags = flags;
	}

	/**
	 * @return true on success, false on error (with errno set if
	 * USE_EPOLL is defined)
	 */
	bool Schedule(unsigned flags) noexcept;

	void Cancel() noexcept {
		Schedule(0);
	}

	bool ScheduleRead() noexcept {
		return Schedule(GetScheduledFlags() | READ);
	}

	bool ScheduleWrite() noexcept {
		return Schedule(GetScheduledFlags() | WRITE);
	}

	void CancelRead() noexcept {
		Schedule(GetScheduledFlags() & ~READ);
	}

	void CancelWrite() noexcept {
		Schedule(GetScheduledFlags() & ~WRITE);
	}

	/**
	 * Schedule only the #IMPLICIT_FLAGS without #READ and #WRITE.
	 * This is not possible with Schedule(), and no other
	 * ScheduleX()/CancelX() method may be called on this object.
	 */
	void ScheduleImplicit() noexcept;

	bool IsReadPending() const noexcept {
		return GetScheduledFlags() & READ;
	}

	bool IsWritePending() const noexcept {
		return GetScheduledFlags() & WRITE;
	}

public:
	void Dispatch() noexcept;
};

#endif
