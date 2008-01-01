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

#include "storedPlaylist.h"
#include "log.h"
#include "path.h"
#include "utils.h"
#include "playlist.h"
#include "ack.h"
#include "command.h"
#include "ls.h"
#include "directory.h"

#include <string.h>
#include <errno.h>

/*
 * Not supporting '/' was done out of laziness, and we should really
 * strive to support it in the future.
 *
 * Not supporting '\r' and '\n' is done out of protocol limitations (and
 * arguably laziness), but bending over head over heels to modify the
 * protocol (and compatibility with all clients) to support idiots who
 * put '\r' and '\n' in filenames isn't going to happen, either.
 */
static int valid_playlist_name(int err_fd, const char *utf8path)
{
	if (strchr(utf8path, '/') ||
	    strchr(utf8path, '\n') ||
	    strchr(utf8path, '\r')) {
		commandError(err_fd, ACK_ERROR_ARG, "playlist name \"%s\" is "
		             "invalid: playlist names may not contain slashes,"
			     " newlines or carriage returns",
		             utf8path);
		return 0;
	}
	return 1;
}

/*
 * converts a path passed from a client into an absolute, FS path
 * paths passed by clients do NOT have file suffixes in them
 */
static void utf8_to_fs_playlist_path(char *path_max_tmp, const char *utf8path)
{
	utf8_to_fs_charset(path_max_tmp, (char *)utf8path);
	rpp2app_r(path_max_tmp, path_max_tmp);
	strncat(path_max_tmp, "." PLAYLIST_FILE_SUFFIX, MPD_PATH_MAX - 1);

	return path_max_tmp;
}

static unsigned int lengthOfStoredPlaylist(StoredPlaylist *sp)
{
	return sp->list->numberOfNodes;
}

static ListNode *nodeOfStoredPlaylist(StoredPlaylist *sp, int index)
{
	int forward;
	ListNode *node;
	int i;

	if (index >= lengthOfStoredPlaylist(sp) || index < 0)
		return NULL;

	if (index > lengthOfStoredPlaylist(sp)/2) {
		forward = 0;
		node = sp->list->lastNode;
		i = lengthOfStoredPlaylist(sp) - 1;
	} else {
		forward = 1;
		node = sp->list->firstNode;
		i = 0;
	}

	while (node != NULL) {
		if (i == index)
			return node;

		if (forward) {
			i++;
			node = node->nextNode;
		} else {
			i--;
			node = node->prevNode;
		}
	}

	return NULL;
}

static void appendSongToStoredPlaylist(StoredPlaylist *sp, Song *song)
{
	char path_max_tmp[MPD_PATH_MAX];

	get_song_url(path_max_tmp, song);
	insertInListWithoutKey(sp->list, xstrdup(path_max_tmp));
}

StoredPlaylist *newStoredPlaylist(const char *utf8name, int fd, int ignoreExisting)
{
	struct stat buf;
	char filename[MPD_PATH_MAX];
	StoredPlaylist *sp;

	if (!valid_playlist_name(fd, utf8name))
		return NULL;

	utf8_to_fs_playlist_path(filename, utf8name);

	if (stat(filename, &buf) == 0 && ! ignoreExisting) {
		commandError(fd, ACK_ERROR_EXIST,
			     "a file or directory already exists with "
			     "the name \"%s\"", utf8name);
		return NULL;
	}
	if (!(sp = malloc(sizeof(*sp))))
		return NULL;

	sp->list = makeList(DEFAULT_FREE_DATA_FUNC, 0);
	sp->fd = fd;
	sp->fspath = xstrdup(filename);

	return sp;
}

/* FIXME - this function is gross */
StoredPlaylist *loadStoredPlaylist(const char *utf8path, int fd)
{
	StoredPlaylist *sp;
	FILE *file;
	char s[MPD_PATH_MAX];
	char path_max_tmp[MPD_PATH_MAX];
	char path_max_tmp2[MPD_PATH_MAX]; /* TODO: cleanup */
	char path_max_tmp3[MPD_PATH_MAX]; /* TODO: cleanup */
	int slength = 0;
	char *temp;
	char *parent;
	int parentlen;
	int tempInt;
	int commentCharFound = 0;
	Song *song;

	if (!valid_playlist_name(fd, utf8path))
		return NULL;

	utf8_to_fs_playlist_path(path_max_tmp, utf8path);

	while (!(file = fopen(path_max_tmp, "r")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", path_max_tmp, strerror(errno));
		return NULL;
	}

	sp = newStoredPlaylist(utf8path, fd, 1);
	if (!sp)
		goto out;

	temp = utf8_to_fs_charset(path_max_tmp2, (char *)utf8path);
	parent = parent_path(path_max_tmp3, temp);
	parentlen = strlen(parent);

	while ((tempInt = fgetc(file)) != EOF) {
		s[slength] = tempInt;
		if (s[slength] == '\n' || s[slength] == '\0') {
			commentCharFound = 0;
			s[slength] = '\0';
			if (s[0] == PLAYLIST_COMMENT)
				commentCharFound = 1;
			if (!strncmp(s, musicDir, strlen(musicDir)) &&
			    s[strlen(musicDir)] == '/') {
				memmove(s, &(s[strlen(musicDir) + 1]),
				        strlen(&(s[strlen(musicDir) + 1])) + 1);
			} else if (parentlen) {
				temp = xstrdup(s);
				strcpy(s, parent);
				strncat(s, "/", MPD_PATH_MAX - parentlen);
				strncat(s, temp, MPD_PATH_MAX - parentlen - 1);
				if (strlen(s) >= MPD_PATH_MAX) {
					commandError(sp->fd,
					             ACK_ERROR_PLAYLIST_LOAD,
					             "\"%s\" is too long", temp);
					free(temp);
					freeStoredPlaylist(sp);
					sp = NULL;
					goto out;
				}
				free(temp);
			}
			slength = 0;
			temp = fs_charset_to_utf8(path_max_tmp, s);
			if (temp && !commentCharFound) {
				song = getSongFromDB(temp);
				if (song) {
					appendSongToStoredPlaylist(sp, song);
					continue;
				}

				if (!isValidRemoteUtf8Url(temp))
					continue;

				song = newSong(temp, SONG_TYPE_URL, NULL);
				if (song) {
					appendSongToStoredPlaylist(sp, song);
					freeJustSong(song);
				}
			}
		} else if (slength == (MPD_PATH_MAX - 1)) {
			s[slength] = '\0';
			commandError(sp->fd, ACK_ERROR_PLAYLIST_LOAD,
				     "line \"%s\" in playlist \"%s\" "
				     "is too long", s, utf8path);
			freeStoredPlaylist(sp);
			sp = NULL;
			goto out;
		} else if (s[slength] != '\r') {
			slength++;
		}
	}

out:
	while (fclose(file) && errno == EINTR);
	return sp;
}

void freeStoredPlaylist(StoredPlaylist *sp)
{
	if (sp->list)
		freeList(sp->list);
	if (sp->fspath)
		free(sp->fspath);

	free(sp);
}

static int moveSongInStoredPlaylist(int fd, StoredPlaylist *sp, int src, int dest)
{
	ListNode *srcNode, *destNode;

	if (src >= lengthOfStoredPlaylist(sp) || dest >= lengthOfStoredPlaylist(sp) || src < 0 || dest < 0 || src == dest) {
		commandError(fd, ACK_ERROR_ARG, "argument out of range");
		return -1;
	}

	srcNode = nodeOfStoredPlaylist(sp, src);
	if (!srcNode)
		return -1;

	destNode = nodeOfStoredPlaylist(sp, dest);

	/* remove src */
	if (srcNode->prevNode)
		srcNode->prevNode->nextNode = srcNode->nextNode;
	else
		sp->list->firstNode = srcNode->nextNode;

	if (srcNode->nextNode)
		srcNode->nextNode->prevNode = srcNode->prevNode;
	else
		sp->list->lastNode = srcNode->prevNode;

	/* this is all a bit complicated - but I tried to
	 * maintain the same order stuff is moved as in the
	 * real playlist */
	if (dest == 0) {
		sp->list->firstNode->prevNode = srcNode;
		srcNode->nextNode = sp->list->firstNode;
		srcNode->prevNode = NULL;
		sp->list->firstNode = srcNode;
	} else if ((dest + 1) == lengthOfStoredPlaylist(sp)) {
		sp->list->lastNode->nextNode = srcNode;
		srcNode->nextNode = NULL;
		srcNode->prevNode = sp->list->lastNode;
		sp->list->lastNode = srcNode;
	} else {
		if (destNode == NULL) {
			/* this shouldn't be happening. */
			return -1;
		}

		if (src > dest) {
			destNode->prevNode->nextNode = srcNode;
			srcNode->prevNode = destNode->prevNode;
			srcNode->nextNode = destNode;
			destNode->prevNode = srcNode;
		} else {
			destNode->nextNode->prevNode = srcNode;
			srcNode->prevNode = destNode;
			srcNode->nextNode = destNode->nextNode;
			destNode->nextNode = srcNode;
		}
	}

	return 0;
}

int moveSongInStoredPlaylistByPath(int fd, const char *utf8path, int src, int dest)
{
	StoredPlaylist *sp = loadStoredPlaylist(utf8path, fd);
	if (!sp) {
		commandError(fd, ACK_ERROR_UNKNOWN, "could not open playlist");
		return -1;
	}

	if (moveSongInStoredPlaylist(fd, sp, src, dest) != 0) {
		freeStoredPlaylist(sp);
		return -1;
	}

	if (writeStoredPlaylist(sp) != 0) {
		commandError(fd, ACK_ERROR_UNKNOWN, "failed to save playlist");
		freeStoredPlaylist(sp);
		return -1;
	}

	freeStoredPlaylist(sp);
	return 0;
}

int removeAllFromStoredPlaylistByPath(int fd, const char *utf8path)
{
	char filename[MPD_PATH_MAX];
	FILE *file;

	if (!valid_playlist_name(fd, utf8path))
		return -1;
	utf8_to_fs_playlist_path(filename, utf8path);

	while (!(file = fopen(filename, "w")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", filename, strerror(errno));
		return -1;
	}

	while (fclose(file) != 0 && errno == EINTR);
	return 0;
}

static int removeOneSongFromStoredPlaylist(int fd, StoredPlaylist *sp, int pos)
{
	ListNode *node = nodeOfStoredPlaylist(sp, pos);
	if (!node) {
		commandError(fd, ACK_ERROR_ARG,
		             "could not find song at position");
		return -1;
	}

	deleteNodeFromList(sp->list, node);

	return 0;
}

int removeOneSongFromStoredPlaylistByPath(int fd, const char *utf8path, int pos)
{
	StoredPlaylist *sp = loadStoredPlaylist(utf8path, fd);
	if (!sp) {
		commandError(fd, ACK_ERROR_UNKNOWN, "could not open playlist");
		return -1;
	}

	if (removeOneSongFromStoredPlaylist(fd, sp, pos) != 0) {
		freeStoredPlaylist(sp);
		return -1;
	}

	if (writeStoredPlaylist(sp) != 0) {
		commandError(fd, ACK_ERROR_UNKNOWN, "failed to save playlist");
		freeStoredPlaylist(sp);
		return -1;
	}

	freeStoredPlaylist(sp);
	return 0;
}

static int writeStoredPlaylistToPath(StoredPlaylist *sp, const char *fspath)
{
	ListNode *node;
	FILE *file;
	char *s;

	if (fspath == NULL)
		return -1;

	while (!(file = fopen(fspath, "w")) && errno == EINTR);
	if (file == NULL) {
		commandError(sp->fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", fspath, strerror(errno));
		return -1;
	}

	node = sp->list->firstNode;
	while (node != NULL) {
		char path_max_tmp[MPD_PATH_MAX];

		s = utf8_to_fs_charset(path_max_tmp, (char *)node->data);
		if (playlist_saveAbsolutePaths && !isValidRemoteUtf8Url(s))
			s = rmp2amp_r(path_max_tmp, s);
		fprintf(file, "%s\n", s);
		node = node->nextNode;
	}

	while (fclose(file) != 0 && errno == EINTR);
	return 0;
}

int writeStoredPlaylist(StoredPlaylist *sp)
{
	return writeStoredPlaylistToPath(sp, sp->fspath);
}

int appendSongToStoredPlaylistByPath(int fd, const char *utf8path, Song *song)
{
	FILE *file;
	char *s;
	char path_max_tmp[MPD_PATH_MAX];
	char path_max_tmp2[MPD_PATH_MAX];

	if (!valid_playlist_name(fd, utf8path))
		return -1;
	utf8_to_fs_playlist_path(path_max_tmp, utf8path);

	while (!(file = fopen(path_max_tmp, "a")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", path_max_tmp, strerror(errno));
		return -1;
	}

	s = utf8_to_fs_charset(path_max_tmp2, get_song_url(path_max_tmp, song));

	if (playlist_saveAbsolutePaths && song->type == SONG_TYPE_FILE)
		s = rmp2amp_r(path_max_tmp, s);

	fprintf(file, "%s\n", s);

	while (fclose(file) != 0 && errno == EINTR);
	return 0;
}

void appendPlaylistToStoredPlaylist(StoredPlaylist *sp, Playlist *playlist)
{
	int i;
	for (i = 0; i < playlist->length; i++)
		appendSongToStoredPlaylist(sp, playlist->songs[i]);
}

int renameStoredPlaylist(int fd, const char *utf8from, const char *utf8to)
{
	struct stat st;
	char from[MPD_PATH_MAX];
	char to[MPD_PATH_MAX];

	if (!valid_playlist_name(fd, utf8from) ||
	    !valid_playlist_name(fd, utf8to))
		return -1;

	utf8_to_fs_playlist_path(from, utf8from);
	utf8_to_fs_playlist_path(to, utf8to);

	if (stat(from, &st) != 0) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "no playlist named \"%s\"", utf8from);
		return -1;
	}

	if (stat(to, &st) == 0) {
		commandError(fd, ACK_ERROR_EXIST, "a file or directory "
			     "already exists with the name \"%s\"", utf8to);
		return -1;
	}

	if (rename(from, to) < 0) {
		commandError(fd, ACK_ERROR_UNKNOWN,
		             "could not rename playlist \"%s\" to \"%s\": %s",
		             utf8from, utf8to, strerror(errno));
		return -1;
	}

	return 0;
}
