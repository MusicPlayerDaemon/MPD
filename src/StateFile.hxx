/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "event/TimeoutMonitor.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#include <string>

struct Partition;
class OutputStream;
class BufferedOutputStream;

class StateFile final : private TimeoutMonitor {
	const AllocatedPath path;
	const std::string path_utf8;

	const unsigned interval;

	Partition &partition;

	/**
	 * These version numbers determine whether we need to save the state
	 * file.  If nothing has changed, we won't let the hard drive spin up.
	 */
	unsigned prev_volume_version, prev_output_version,
		prev_playlist_version;

public:
	static constexpr unsigned DEFAULT_INTERVAL = 2 * 60;

	StateFile(AllocatedPath &&path, unsigned interval,
		  Partition &partition, EventLoop &loop);

	void Read();
	void Write();

	/**
	 * Schedules a write if MPD's state was modified.
	 */
	void CheckModified();

private:
	bool Write(OutputStream &os, Error &error);
	void Write(BufferedOutputStream &os);

	/**
	 * Save the current state versions for use with IsModified().
	 */
	void RememberVersions();

	/**
	 * Check if MPD's state was modified since the last
	 * RememberVersions() call.
	 */
	gcc_pure
	bool IsModified() const;

	/* virtual methods from TimeoutMonitor */
	virtual void OnTimeout() override;
};

#endif /* STATE_FILE_H */
