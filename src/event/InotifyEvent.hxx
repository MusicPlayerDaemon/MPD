// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "PipeEvent.hxx"

#include <exception>

/**
 * Handler for #InotifyEvent.
 */
class InotifyHandler {
public:
	/**
	 * An inotify event was received.
	 *
	 * @param wd the watch descriptor returned by
	 * InotifyEvent::AddWatch().
	 */
	virtual void OnInotify(int wd, unsigned mask, const char *name) = 0;

	/**
	 * An (permanent) inotify error has occurred, and the
	 * #InotifyEvent has been closed.
	 */
	virtual void OnInotifyError(std::exception_ptr error) noexcept = 0;
};

/**
 * #EventLoop integration for Linux inotify.
 */
class InotifyEvent final {
	PipeEvent event;

	InotifyHandler &handler;

public:
	/**
	 * Create an inotify file descriptor add register it in the
	 * #EventLoop.
	 *
	 * Throws on error.
	 */
	InotifyEvent(EventLoop &event_loop, InotifyHandler &_handler);

	~InotifyEvent() noexcept;

	EventLoop &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Is the inotify file descriptor still open?
	 */
	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	/**
	 * Re-enable polling the inotify file descriptor after it was
	 * disabled by Disable().
	 */
	void Enable() noexcept {
		event.ScheduleRead();
	}

	/**
	 * Disable polling the inotify file descriptor.  Can be
	 * re-enabled by Enable().
	 */
	void Disable() noexcept {
		event.Cancel();
	}

	/**
	 * Permanently close the inotify file descriptor.  Further
	 * method calls not allowed after that.
	 */
	void Close() noexcept {
		event.Close();
	}

	/**
	 * Register a new path to be watched.
	 *
	 * Throws on error.
	 *
	 * @return a watch descriptor
	 */
	int AddWatch(const char *pathname, uint32_t mask);

	/**
	 * Like AddWatch(), but returns -1 instead of throwing on
	 * error.
	 */
	int TryAddWatch(const char *pathname, uint32_t mask) noexcept;

	/**
	 * Wrapper for AddWatch(pathname, IN_MODIFY).
	 */
	int AddModifyWatch(const char *pathname);

	/**
	 * Stop watching the given watch descriptor.
	 *
	 * @param wd a watch descriptor returned by AddWatch()
	 */
	void RemoveWatch(int wd) noexcept;

private:
	void OnInotifyReady(unsigned) noexcept;
};
