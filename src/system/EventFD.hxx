// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef EVENT_FD_HXX
#define EVENT_FD_HXX

#include "io/UniqueFileDescriptor.hxx"

/**
 * A class that wraps eventfd().
 */
class EventFD {
	UniqueFileDescriptor fd;

public:
	/**
	 * Throws on error.
	 */
	EventFD();

	FileDescriptor Get() const noexcept {
		return fd;
	}

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

#endif
