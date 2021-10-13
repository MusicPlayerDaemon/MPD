/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "StateFileConfig.hxx"
#include "event/FarTimerEvent.hxx"
#include "config.h"

#include <string>

struct Partition;
class OutputStream;
class BufferedOutputStream;

class StateFile final {
	const StateFileConfig config;

	const std::string path_utf8;

	FarTimerEvent timer_event;

	Partition &partition;

	/**
	 * These version numbers determine whether we need to save the state
	 * file.  If nothing has changed, we won't let the hard drive spin up.
	 */
	unsigned prev_volume_version = 0, prev_output_version = 0,
		prev_playlist_version = 0;

#ifdef ENABLE_DATABASE
	unsigned prev_storage_version = 0;
#endif

public:
	StateFile(StateFileConfig &&_config,
		  Partition &partition, EventLoop &loop);

	void Read();
	void Write();

	/**
	 * Schedules a write if MPD's state was modified.
	 */
	void CheckModified() noexcept;

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
	[[gnu::pure]]
	bool IsModified() const noexcept;

	/* callback for #timer_event */
	void OnTimeout() noexcept;
};

#endif /* STATE_FILE_H */
