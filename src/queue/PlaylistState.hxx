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

#ifndef MPD_PLAYLIST_STATE_HXX
#define MPD_PLAYLIST_STATE_HXX

struct StateFileConfig;
struct playlist;
class PlayerControl;
class LineReader;
class BufferedOutputStream;
class SongLoader;

void
playlist_state_save(BufferedOutputStream &os, const playlist &playlist,
		    PlayerControl &pc);

bool
playlist_state_restore(const StateFileConfig &config,
		       const char *line, LineReader &file,
		       const SongLoader &song_loader,
		       playlist &playlist, PlayerControl &pc);

/**
 * Generates a hash number for the current state of the playlist and
 * the playback options.  This is used by timer_save_state_file() to
 * determine whether the state has changed and the state file should
 * be saved.
 */
unsigned
playlist_state_get_hash(const playlist &playlist,
			PlayerControl &c);

#endif
