// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
