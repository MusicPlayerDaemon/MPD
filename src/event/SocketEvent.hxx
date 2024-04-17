// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "BackendEvents.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

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
class SocketEvent final
	: IntrusiveListHook<IntrusiveHookMode::NORMAL>,
	  public EventPollBackendEvents
{
	friend class EventLoop;
	friend struct IntrusiveListBaseHookTraits<SocketEvent>;

	EventLoop &loop;

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
	/**
	 * These flags are always reported by epoll_wait() and don't
	 * need to be registered with epoll_ctl().
	 */
	static constexpr unsigned IMPLICIT_FLAGS = ERROR|HANGUP;

	SocketEvent(EventLoop &_loop, Callback _callback,
		    SocketDescriptor _fd=SocketDescriptor::Undefined()) noexcept
		:loop(_loop),
		 callback(_callback),
		 fd(_fd) {}

	~SocketEvent() noexcept {
		Cancel();
	}

	SocketEvent(const SocketEvent &) = delete;
	SocketEvent &operator=(const SocketEvent &) = delete;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsDefined() const noexcept {
		return fd.IsDefined();
	}

	SocketDescriptor GetSocket() const noexcept {
		return fd;
	}

	SocketDescriptor ReleaseSocket() noexcept {
		Cancel();
		return std::exchange(fd, SocketDescriptor::Undefined());
	}

	void Open(SocketDescriptor fd) noexcept;

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

	unsigned GetReadyFlags() const noexcept {
		return ready_flags;
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
		/* IMPLICIT_FLAGS is erased from the flags so
		   CancelRead() after ScheduleRead() cancels the whole
		   event instead of leaving IMPLICIT_FLAGS
		   scheduled */
		Schedule(GetScheduledFlags() & ~(READ|IMPLICIT_FLAGS));
	}

	void CancelWrite() noexcept {
		Schedule(GetScheduledFlags() & ~(WRITE|IMPLICIT_FLAGS));
	}

	/**
	 * Schedule only the #IMPLICIT_FLAGS without #READ and #WRITE.
	 */
	void ScheduleImplicit() noexcept {
		Schedule(IMPLICIT_FLAGS);
	}

	bool IsReadPending() const noexcept {
		return GetScheduledFlags() & READ;
	}

	bool IsWritePending() const noexcept {
		return GetScheduledFlags() & WRITE;
	}

private:
	/**
	 * Dispatch the events that were passed to SetReadyFlags().
	 */
	void Dispatch() noexcept;
};
