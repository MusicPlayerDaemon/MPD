/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "playlist_error.h"

#include <stdbool.h>

struct player_control;

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
};

/** the global playlist object */
extern struct playlist g_playlist;

void
playlist_global_init(void);

void
playlist_global_finish(void);

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

void
playlist_clear(struct playlist *playlist, struct player_control *pc);

/**
 * Appends a local file (outside the music database) to the playlist.
 *
 * Note: the caller is responsible for checking permissions.
 */
enum playlist_result
playlist_append_file(struct playlist *playlist, struct player_control *pc,
		     const char *path_fs, unsigned *added_id);

enum playlist_result
playlist_append_uri(struct playlist *playlist, struct player_control *pc,
		    const char *file, unsigned *added_id);

enum playlist_result
playlist_append_song(struct playlist *playlist, struct player_control *pc,
		  struct song *song, unsigned *added_id);

enum playlist_result
playlist_delete(struct playlist *playlist, struct player_control *pc,
		unsigned song);

/**
 * Deletes a range of songs from the playlist.
 *
 * @param start the position of the first song to delete
 * @param end the position after the last song to delete
 */
enum playlist_result
playlist_delete_range(struct playlist *playlist, struct player_control *pc,
		      unsigned start, unsigned end);

enum playlist_result
playlist_delete_id(struct playlist *playlist, struct player_control *pc,
		   unsigned song);

void
playlist_stop(struct playlist *playlist, struct player_control *pc);

enum playlist_result
playlist_play(struct playlist *playlist, struct player_control *pc,
	      int song);

enum playlist_result
playlist_play_id(struct playlist *playlist, struct player_control *pc,
		 int song);

void
playlist_next(struct playlist *playlist, struct player_control *pc);

void
playlist_sync(struct playlist *playlist, struct player_control *pc);

void
playlist_previous(struct playlist *playlist, struct player_control *pc);

void
playlist_shuffle(struct playlist *playlist, struct player_control *pc,
		 unsigned start, unsigned end);

void
playlist_delete_song(struct playlist *playlist, struct player_control *pc,
		     const struct song *song);

enum playlist_result
playlist_move_range(struct playlist *playlist, struct player_control *pc,
		    unsigned start, unsigned end, int to);

enum playlist_result
playlist_move_id(struct playlist *playlist, struct player_control *pc,
		 unsigned id, int to);

enum playlist_result
playlist_swap_songs(struct playlist *playlist, struct player_control *pc,
		    unsigned song1, unsigned song2);

enum playlist_result
playlist_swap_songs_id(struct playlist *playlist, struct player_control *pc,
		       unsigned id1, unsigned id2);

enum playlist_result
playlist_set_priority(struct playlist *playlist, struct player_control *pc,
		      unsigned start_position, unsigned end_position,
		      uint8_t priority);

enum playlist_result
playlist_set_priority_id(struct playlist *playlist, struct player_control *pc,
			 unsigned song_id, uint8_t priority);

bool
playlist_get_repeat(const struct playlist *playlist);

void
playlist_set_repeat(struct playlist *playlist, struct player_control *pc,
		    bool status);

bool
playlist_get_random(const struct playlist *playlist);

void
playlist_set_random(struct playlist *playlist, struct player_control *pc,
		    bool status);

bool
playlist_get_single(const struct playlist *playlist);

void
playlist_set_single(struct playlist *playlist, struct player_control *pc,
		    bool status);

bool
playlist_get_consume(const struct playlist *playlist);

void
playlist_set_consume(struct playlist *playlist, bool status);

int
playlist_get_current_song(const struct playlist *playlist);

int
playlist_get_next_song(const struct playlist *playlist);

unsigned
playlist_get_song_id(const struct playlist *playlist, unsigned song);

int
playlist_get_length(const struct playlist *playlist);

unsigned long
playlist_get_version(const struct playlist *playlist);

enum playlist_result
playlist_seek_song(struct playlist *playlist, struct player_control *pc,
		   unsigned song, float seek_time);

enum playlist_result
playlist_seek_song_id(struct playlist *playlist, struct player_control *pc,
		       unsigned id, float seek_time);

/**
 * Seek within the current song.  Fails if MPD is not currently
 * playing.
 *
 * @param time the time in seconds
 * @param relative if true, then the specified time is relative to the
 * current position
 */
enum playlist_result
playlist_seek_current(struct playlist *playlist, struct player_control *pc,
		      float seek_time, bool relative);

void
playlist_increment_version_all(struct playlist *playlist);

#endif
