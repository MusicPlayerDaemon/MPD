// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
