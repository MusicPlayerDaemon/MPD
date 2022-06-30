/*
 * Copyright 2022 CM4all GmbH
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
