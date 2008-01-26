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

#include "../config.h"

#include "dbUtils.h"

#include <stdio.h>
#include <sys/param.h>
#include <time.h>

#define PLAYLIST_FILE_SUFFIX 	"m3u"
#define PLAYLIST_COMMENT	'#'

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

int clearPlaylist(int fd);

int clearStoredPlaylist(int fd, char *utf8file);

int addToPlaylist(int fd, char *file, int printId);

int addToStoredPlaylist(int fd, char *file, char *utf8file);

int addSongToPlaylist(int fd, Song * song, int printId);

int showPlaylist(int fd);

int deleteFromPlaylist(int fd, int song);

int deleteFromPlaylistById(int fd, int song);

int playlistInfo(int fd, int song);

int playlistId(int fd, int song);

int stopPlaylist(int fd);

int playPlaylist(int fd, int song, int stopOnError);

int playPlaylistById(int fd, int song, int stopOnError);

int nextSongInPlaylist(int fd);

void syncPlayerAndPlaylist(void);

int previousSongInPlaylist(int fd);

int shufflePlaylist(int fd);

int savePlaylist(int fd, char *utf8file);

int deletePlaylist(int fd, char *utf8file);

int deletePlaylistById(int fd, char *utf8file);

void deleteASongFromPlaylist(Song * song);

int moveSongInPlaylist(int fd, int from, int to);

int moveSongInPlaylistById(int fd, int id, int to);

int swapSongsInPlaylist(int fd, int song1, int song2);

int swapSongsInPlaylistById(int fd, int id1, int id2);

int loadPlaylist(int fd, char *utf8file);

int getPlaylistRepeatStatus(void);

int setPlaylistRepeatStatus(int fd, int status);

int getPlaylistRandomStatus(void);

int setPlaylistRandomStatus(int fd, int status);

int getPlaylistCurrentSong(void);

int getPlaylistSongId(int song);

int getPlaylistLength(void);

unsigned long getPlaylistVersion(void);

void playPlaylistIfPlayerStopped(void);

int seekSongInPlaylist(int fd, int song, float time);

int seekSongInPlaylistById(int fd, int id, float time);

void playlistVersionChange(void);

int playlistChanges(int fd, mpd_uint32 version);

int playlistChangesPosId(int fd, mpd_uint32 version);

int PlaylistInfo(int fd, char *utf8file, int detail);

void searchForSongsInPlaylist(int fd, int numItems, LocateTagItem * items);

void findSongsInPlaylist(int fd, int numItems, LocateTagItem * items);

#endif
