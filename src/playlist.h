/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_PLAYLIST_H
#define MPD_PLAYLIST_H

#include "queue.h"

#include <stdbool.h>
#include <stdio.h>

#define PLAYLIST_COMMENT	'#'

enum playlist_result {
	PLAYLIST_RESULT_SUCCESS,
	PLAYLIST_RESULT_ERRNO,
	PLAYLIST_RESULT_DENIED,
	PLAYLIST_RESULT_NO_SUCH_SONG,
	PLAYLIST_RESULT_NO_SUCH_LIST,
	PLAYLIST_RESULT_LIST_EXISTS,
	PLAYLIST_RESULT_BAD_NAME,
	PLAYLIST_RESULT_BAD_RANGE,
	PLAYLIST_RESULT_NOT_PLAYING,
	PLAYLIST_RESULT_TOO_LARGE,
	PLAYLIST_RESULT_DISABLED,
};

struct playlist {
	/**
	 * The song queue - it contains the "real" playlist.
	 */
	struct queue queue;

	/**
	 * This value is true if the player is currently playing (or
	 * should be playing).
	 */
	bool playing;

	/**
	 * If true, then any error is fatal; if false, MPD will
	 * attempt to play the next song on non-fatal errors.  During
	 * seeking, this flag is set.
	 */
	bool stop_on_error;

	/**
	 * Number of errors since playback was started.  If this
	 * number exceeds the length of the playlist, MPD gives up,
	 * because all songs have been tried.
	 */
	unsigned error_count;

	/**
	 * The "current song pointer".  This is the song which is
	 * played when we get the "play" command.  It is also the song
	 * which is currently being played.
	 */
	int current;

	/**
	 * The "next" song to be played, when the current one
	 * finishes.  The decoder thread may start decoding and
	 * buffering it, while the "current" song is still playing.
	 *
	 * This variable is only valid if #playing is true.
	 */
	int queued;

	/**
	 * This timer tracks the time elapsed since the last "prev"
	 * command.  If that is less than one second ago, "prev" jumps
	 * to the previous song instead of rewinding the current song.
	 */
	GTimer *prev_elapsed;
};

/** the global playlist object */
extern struct playlist g_playlist;

void initPlaylist(void);

void finishPlaylist(void);

void
playlist_init(struct playlist *playlist);

void
playlist_finish(struct playlist *playlist);

void
playlist_tag_changed(struct playlist *playlist);

/**
 * Returns the "queue" object of the global playlist instance.
 */
static inline const struct queue *
playlist_get_queue(const struct playlist *playlist)
{
	return &playlist->queue;
}

void readPlaylistState(FILE *);

void savePlaylistState(FILE *);

void clearPlaylist(struct playlist *playlist);

#ifndef WIN32
/**
 * Appends a local file (outside the music database) to the playlist,
 * but only if the file's owner is equal to the specified uid.
 */
enum playlist_result
playlist_append_file(struct playlist *playlist, const char *path, int uid,
		     unsigned *added_id);
#endif

enum playlist_result
addToPlaylist(struct playlist *playlist, const char *file, unsigned *added_id);

enum playlist_result
addSongToPlaylist(struct playlist *playlist,
		  struct song *song, unsigned *added_id);

enum playlist_result
deleteFromPlaylist(struct playlist *playlist, unsigned song);

enum playlist_result
deleteFromPlaylistById(struct playlist *playlist, unsigned song);

void stopPlaylist(struct playlist *playlist);

enum playlist_result
playPlaylist(struct playlist *playlist, int song);

enum playlist_result
playPlaylistById(struct playlist *playlist, int song);

void nextSongInPlaylist(struct playlist *playlist);

void syncPlayerAndPlaylist(struct playlist *playlist);

void previousSongInPlaylist(struct playlist *playlist);

void shufflePlaylist(struct playlist *playlist, unsigned start, unsigned end);

void
deleteASongFromPlaylist(struct playlist *playlist, const struct song *song);

enum playlist_result
moveSongRangeInPlaylist(struct playlist *playlist, unsigned start, unsigned end, int to);

enum playlist_result
moveSongInPlaylistById(struct playlist *playlist, unsigned id, int to);

enum playlist_result
swapSongsInPlaylist(struct playlist *playlist, unsigned song1, unsigned song2);

enum playlist_result
swapSongsInPlaylistById(struct playlist *playlist, unsigned id1, unsigned id2);

bool
getPlaylistRepeatStatus(const struct playlist *playlist);

void setPlaylistRepeatStatus(struct playlist *playlist, bool status);

bool
getPlaylistRandomStatus(const struct playlist *playlist);

void setPlaylistRandomStatus(struct playlist *playlist, bool status);

bool
getPlaylistSingleStatus(const struct playlist *playlist);

void setPlaylistSingleStatus(struct playlist *playlist, bool status);

bool
getPlaylistConsumeStatus(const struct playlist *playlist);

void setPlaylistConsumeStatus(struct playlist *playlist, bool status);

int getPlaylistCurrentSong(const struct playlist *playlist);

int getPlaylistNextSong(const struct playlist *playlist);

unsigned
getPlaylistSongId(const struct playlist *playlist, unsigned song);

int getPlaylistLength(const struct playlist *playlist);

unsigned long
getPlaylistVersion(const struct playlist *playlist);

enum playlist_result
seekSongInPlaylist(struct playlist *playlist, unsigned song, float seek_time);

enum playlist_result
seekSongInPlaylistById(struct playlist *playlist,
		       unsigned id, float seek_time);

void playlistVersionChange(struct playlist *playlist);

int is_valid_playlist_name(const char *utf8path);

#endif
