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
storage_state_save(BufferedOutputStream &os, const Instance &instance);

bool
storage_state_restore(const char *line, LineReader &file,
		      Instance &instance);

/**
 * Generates a hash number for the current state of the composite storage.
 * This is used by timer_save_state_file() to determine whether the state
 * has changed and the state file should be saved.
 */
unsigned
storage_state_get_hash(const Instance &instance);

#endif
