// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_EVENT_PIPE_HXX
#define MPD_EVENT_PIPE_HXX

#ifdef _WIN32
#include "net/UniqueSocketDescriptor.hxx"
#else
#include "io/UniqueFileDescriptor.hxx"
#endif

/**
 * A pipe that can be used to trigger an event to the read side.
 *
 * Errors in the constructor are fatal.
 */
class EventPipe {
#ifdef _WIN32
	UniqueSocketDescriptor r, w;
#else
	UniqueFileDescriptor r, w;
#endif

public:
	/**
	 * Throws on error.
	 */
	EventPipe();

	~EventPipe() noexcept;

	EventPipe(const EventPipe &other) = delete;
	EventPipe &operator=(const EventPipe &other) = delete;

#ifdef _WIN32
	SocketDescriptor Get() const noexcept {
		return r;
	}
#else
	FileDescriptor Get() const noexcept {
		return r;
	}
#endif

	/**
	 * Checks if Write() was called at least once since the last
	 * Read() call.
	 */
	bool Read() noexcept;

	/**
	 * Wakes up the reader.  Multiple calls to this function will
	 * be combined to one wakeup.
	 */
	void Write() noexcept;
};

#endif /* MAIN_NOTIFY_H */
