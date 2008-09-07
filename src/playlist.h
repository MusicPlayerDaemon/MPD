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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "locate.h"

#define PLAYLIST_FILE_SUFFIX 	"m3u"
#define PLAYLIST_COMMENT	'#'

enum playlist_result {
	PLAYLIST_RESULT_SUCCESS,
	PLAYLIST_RESULT_ERRNO,
	PLAYLIST_RESULT_NO_SUCH_SONG,
	PLAYLIST_RESULT_NO_SUCH_LIST,
	PLAYLIST_RESULT_LIST_EXISTS,
	PLAYLIST_RESULT_BAD_NAME,
	PLAYLIST_RESULT_BAD_RANGE,
	PLAYLIST_RESULT_NOT_PLAYING,
	PLAYLIST_RESULT_TOO_LARGE
};

typedef struct _Playlist {
	Song **songs;
	/* holds version a song was modified on */
	mpd_uint32 *songMod;
	int *order;
	int *positionToId;
	int *idToPosition;
	int length;
	int current;
	int queued;
	int repeat;
	int random;
	mpd_uint32 version;
} Playlist;

extern int playlist_saveAbsolutePaths;

extern int playlist_max_length;

void initPlaylist(void);

void finishPlaylist(void);

void readPlaylistState(FILE *);

void savePlaylistState(FILE *);

void clearPlaylist(void);

int clearStoredPlaylist(const char *utf8file);

enum playlist_result addToPlaylist(const char *file, int *added_id);

int addToStoredPlaylist(const char *file, const char *utf8file);

enum playlist_result addSongToPlaylist(Song * song, int *added_id);

void showPlaylist(int fd);

enum playlist_result deleteFromPlaylist(int song);

enum playlist_result deleteFromPlaylistById(int song);

enum playlist_result playlistInfo(int fd, int song);

enum playlist_result playlistId(int fd, int song);

void stopPlaylist(void);

enum playlist_result playPlaylist(int song, int stopOnError);

enum playlist_result playPlaylistById(int song, int stopOnError);

void nextSongInPlaylist(void);

void syncPlayerAndPlaylist(void);

void previousSongInPlaylist(void);

void shufflePlaylist(void);

enum playlist_result savePlaylist(const char *utf8file);

enum playlist_result deletePlaylist(const char *utf8file);

void deleteASongFromPlaylist(Song * song);

enum playlist_result moveSongInPlaylist(int from, int to);

enum playlist_result moveSongInPlaylistById(int id, int to);

enum playlist_result swapSongsInPlaylist(int song1, int song2);

enum playlist_result swapSongsInPlaylistById(int id1, int id2);

enum playlist_result loadPlaylist(int fd, const char *utf8file);

int getPlaylistRepeatStatus(void);

void setPlaylistRepeatStatus(int status);

int getPlaylistRandomStatus(void);

void setPlaylistRandomStatus(int status);

int getPlaylistCurrentSong(void);

int getPlaylistSongId(int song);

int getPlaylistLength(void);

unsigned long getPlaylistVersion(void);

void playPlaylistIfPlayerStopped(void);

enum playlist_result seekSongInPlaylist(int song, float seek_time);

enum playlist_result seekSongInPlaylistById(int id, float seek_time);

void playlistVersionChange(void);

int playlistChanges(int fd, mpd_uint32 version);

int playlistChangesPosId(int fd, mpd_uint32 version);

int PlaylistInfo(int fd, const char *utf8file, int detail);

void searchForSongsInPlaylist(int fd, int numItems, LocateTagItem * items);

void findSongsInPlaylist(int fd, int numItems, LocateTagItem * items);

int is_valid_playlist_name(const char *utf8path);

#endif
