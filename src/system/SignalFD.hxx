// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef SIGNAL_FD_HXX
#define SIGNAL_FD_HXX

#include "io/UniqueFileDescriptor.hxx"

#include <csignal>

/**
 * A class that wraps signalfd().
 */
class SignalFD {
	UniqueFileDescriptor fd;

public:
	/**
	 * Create the signalfd or update its mask.
	 *
	 * Throws on error.
	 */
	void Create(const sigset_t &mask);

	void Close() noexcept {
		fd.Close();
	}

	int Get() const noexcept {
		return fd.Get();
	}

	/**
	 * Read the next signal from the file descriptor.  Returns the
	 * signal number on success or -1 if there are no more
	 * signals.
	 */
	int Read() noexcept;
};

#endif
