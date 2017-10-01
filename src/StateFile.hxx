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

#ifndef MPD_STATE_FILE_HXX
#define MPD_STATE_FILE_HXX

#include "event/TimerEvent.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#include <string>
#include <chrono>

struct Partition;
class OutputStream;
class BufferedOutputStream;

class StateFile final {
	const AllocatedPath path;
	const std::string path_utf8;

	const std::chrono::steady_clock::duration interval;
	TimerEvent timer_event;

	Partition &partition;

	/**
	 * These version numbers determine whether we need to save the state
	 * file.  If nothing has changed, we won't let the hard drive spin up.
	 */
	unsigned prev_volume_version = 0, prev_output_version = 0,
		prev_playlist_version = 0;

public:
	static constexpr std::chrono::steady_clock::duration DEFAULT_INTERVAL = std::chrono::minutes(2);

	StateFile(AllocatedPath &&path, std::chrono::steady_clock::duration interval,
		  Partition &partition, EventLoop &loop);

	void Read();
	void Write();

	/**
	 * Schedules a write if MPD's state was modified.
	 */
	void CheckModified();

private:
	void Write(OutputStream &os);
	void Write(BufferedOutputStream &os);

	/**
	 * Save the current state versions for use with IsModified().
	 */
	void RememberVersions() noexcept;

	/**
	 * Check if MPD's state was modified since the last
	 * RememberVersions() call.
	 */
	gcc_pure
	bool IsModified() const noexcept;

	/* callback for #timer_event */
	void OnTimeout();
};

#endif /* STATE_FILE_H */
