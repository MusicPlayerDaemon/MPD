// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_SAVE_H
#define MPD_PLAYLIST_SAVE_H

struct Queue;
struct playlist;
class BufferedOutputStream;
class DetachedSong;

void
playlist_print_song(BufferedOutputStream &os, const DetachedSong &song);

void
playlist_print_uri(BufferedOutputStream &os, const char *uri);

enum class PlaylistSaveMode {
	CREATE,
	APPEND,
	REPLACE
};

void
spl_save_queue(const char *name_utf8, PlaylistSaveMode save_mode, const Queue &queue);

/**
 * Saves a playlist object into a stored playlist file.
 */
void
spl_save_playlist(const char *name_utf8, PlaylistSaveMode save_mode, const playlist &playlist);

#endif
