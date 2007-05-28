/* the Music Player Daemon (MPD)
 * Copyright (C) 2007 by Warren Dukes (warren.dukes@gmail.com)
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

#ifndef STORED_PLAYLIST_H
#define STORED_PLAYLIST_H

#include "song.h"
#include "list.h"
#include "playlist.h"

typedef struct _storedPlaylist {
	List *list;
	unsigned int length;
	char *fspath;
	int fd;
} StoredPlaylist;

StoredPlaylist *newStoredPlaylist(const char *filename, int fd, int ignoreExisting);
StoredPlaylist *loadStoredPlaylist(const char *utf8path, int fd);
void freeStoredPlaylist(StoredPlaylist *sp);

int moveSongInStoredPlaylistByPath(int fd, const char *utf8path, int src, int dest);
int removeAllFromStoredPlaylistByPath(int fd, const char *utf8path);
int removeOneSongFromStoredPlaylistByPath(int fd, const char *utf8path, int pos);

int writeStoredPlaylist(StoredPlaylist *sp);

int appendSongToStoredPlaylistByPath(int fd, const char *utf8path, Song *song);
void appendPlaylistToStoredPlaylist(StoredPlaylist *sp, Playlist *playlist);

int renameStoredPlaylist(int fd, const char *utf8from, const char *utf8to);

#endif
