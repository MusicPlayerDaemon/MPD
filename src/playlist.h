/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_PLAYLIST_H
#define MPD_PLAYLIST_H

#include "locate.h"
#include "queue.h"

#include <stdbool.h>
#include <stdio.h>

#define PLAYLIST_COMMENT	'#'

struct client;

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
};

extern bool playlist_saveAbsolutePaths;

extern unsigned playlist_max_length;

void initPlaylist(void);

void finishPlaylist(void);

void readPlaylistState(FILE *);

void savePlaylistState(FILE *);

void clearPlaylist(void);

#ifndef WIN32
/**
 * Appends a local file (outside the music database) to the playlist,
 * but only if the file's owner is equal to the specified uid.
 */
enum playlist_result
playlist_append_file(const char *path, int uid, unsigned *added_id);
#endif

enum playlist_result addToPlaylist(const char *file, unsigned *added_id);

enum playlist_result
addSongToPlaylist(struct song *song, unsigned *added_id);

void showPlaylist(struct client *client);

enum playlist_result deleteFromPlaylist(unsigned song);

enum playlist_result deleteFromPlaylistById(unsigned song);

/**
 * Send detailed information about a range of songs in the playlist to
 * a client.
 *
 * @param client the client which has requested information
 * @param start the index of the first song (including)
 * @param end the index of the last song (excluding)
 */
enum playlist_result
playlistInfo(struct client *client, unsigned start, unsigned end);

enum playlist_result playlistId(struct client *client, int song);

void stopPlaylist(void);

enum playlist_result playPlaylist(int song);

enum playlist_result playPlaylistById(int song);

void nextSongInPlaylist(void);

void syncPlayerAndPlaylist(void);

void previousSongInPlaylist(void);

void shufflePlaylist(void);

enum playlist_result savePlaylist(const char *utf8file);

void
deleteASongFromPlaylist(const struct song *song);

enum playlist_result moveSongInPlaylist(unsigned from, int to);

enum playlist_result moveSongInPlaylistById(unsigned id, int to);

enum playlist_result swapSongsInPlaylist(unsigned song1, unsigned song2);

enum playlist_result swapSongsInPlaylistById(unsigned id1, unsigned id2);

enum playlist_result loadPlaylist(const char *utf8file);

bool getPlaylistRepeatStatus(void);

void setPlaylistRepeatStatus(bool status);

bool getPlaylistRandomStatus(void);

void setPlaylistRandomStatus(bool status);

int getPlaylistCurrentSong(void);

unsigned getPlaylistSongId(unsigned song);

int getPlaylistLength(void);

unsigned long getPlaylistVersion(void);

enum playlist_result seekSongInPlaylist(unsigned song, float seek_time);

enum playlist_result seekSongInPlaylistById(unsigned id, float seek_time);

void playlistVersionChange(void);

int playlistChanges(struct client *client, uint32_t version);

int playlistChangesPosId(struct client *client, uint32_t version);

void
searchForSongsInPlaylist(struct client *client,
			 unsigned numItems, const LocateTagItem *items);

void
findSongsInPlaylist(struct client *client,
		    unsigned numItems, const LocateTagItem *items);

int is_valid_playlist_name(const char *utf8path);

#endif
