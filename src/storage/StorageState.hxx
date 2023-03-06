// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Saving and loading the playlist to/from the state file.
 *
 */

#ifndef MPD_STORAGE_STATE_HXX
#define MPD_STORAGE_STATE_HXX

struct Instance;
class BufferedOutputStream;
class LineReader;

void
storage_state_save(BufferedOutputStream &os,
		   const Instance &instance);

bool
storage_state_restore(const char *line, LineReader &file,
		      Instance &instance) noexcept;

/**
 * Generates a hash number for the current state of the composite storage.
 * This is used by timer_save_state_file() to determine whether the state
 * has changed and the state file should be saved.
 */
[[gnu::pure]]
unsigned
storage_state_get_hash(const Instance &instance) noexcept;

#endif
