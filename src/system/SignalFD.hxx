/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_SIGNAL_FD_HXX
#define MPD_SIGNAL_FD_HXX

#include "check.h"
#include "UniqueFileDescriptor.hxx"

#include <signal.h>

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
