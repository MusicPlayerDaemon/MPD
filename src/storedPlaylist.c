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
#include "path.h"
#include "utils.h"
#include "ack.h"
#include "command.h"
#include "ls.h"
#include "directory.h"
#include "os_compat.h"

static ListNode *nodeOfStoredPlaylist(List *list, int idx)
{
	int forward;
	ListNode *node;
	int i;

	if (idx >= list->numberOfNodes || idx < 0)
		return NULL;

	if (idx > (list->numberOfNodes/2)) {
		forward = 0;
		node = list->lastNode;
		i = list->numberOfNodes - 1;
	} else {
		forward = 1;
		node = list->firstNode;
		i = 0;
	}

	while (node != NULL) {
		if (i == idx)
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

static int writeStoredPlaylistToPath(int fd, List *list, const char *fspath)
{
	ListNode *node;
	FILE *file;
	char *s;

	assert(fspath != NULL);

	while (!(file = fopen(fspath, "w")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", fspath, strerror(errno));
		return -1;
	}

	node = list->firstNode;
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

List *loadStoredPlaylist(int fd, const char *utf8path)
{
	List *list;
	FILE *file;
	char buffer[MPD_PATH_MAX];
	char path_max_tmp[MPD_PATH_MAX];
	const size_t musicDir_len = strlen(musicDir);

	if (!valid_playlist_name(fd, utf8path))
		return NULL;

	utf8_to_fs_playlist_path(path_max_tmp, utf8path);
	while (!(file = fopen(path_max_tmp, "r")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", path_max_tmp, strerror(errno));
		return NULL;
	}

	list = makeList(DEFAULT_FREE_DATA_FUNC, 0);

	while (myFgets(buffer, sizeof(buffer), file)) {
		char *s = buffer;
		Song *song;

		if (*s == PLAYLIST_COMMENT)
			continue;
		if (s[musicDir_len] == '/' &&
		    !strncmp(s, musicDir, musicDir_len))
			memmove(s, s + musicDir_len + 1,
				strlen(s + musicDir_len + 1) + 1);
		if ((song = getSongFromDB(s))) {
			get_song_url(path_max_tmp, song);
			insertInListWithoutKey(list, xstrdup(path_max_tmp));
		} else if (isValidRemoteUtf8Url(s))
			insertInListWithoutKey(list, xstrdup(s));

		if (list->numberOfNodes >= playlist_max_length)
			break;
	}

	while (fclose(file) && errno == EINTR);
	return list;
}

static int moveSongInStoredPlaylist(int fd, List *list, int src, int dest)
{
	ListNode *srcNode, *destNode;

	if (src >= list->numberOfNodes || dest >= list->numberOfNodes ||
	    src < 0 || dest < 0 || src == dest) {
		commandError(fd, ACK_ERROR_ARG, "argument out of range");
		return -1;
	}

	srcNode = nodeOfStoredPlaylist(list, src);
	if (!srcNode)
		return -1;

	destNode = nodeOfStoredPlaylist(list, dest);

	/* remove src */
	if (srcNode->prevNode)
		srcNode->prevNode->nextNode = srcNode->nextNode;
	else
		list->firstNode = srcNode->nextNode;

	if (srcNode->nextNode)
		srcNode->nextNode->prevNode = srcNode->prevNode;
	else
		list->lastNode = srcNode->prevNode;

	/* this is all a bit complicated - but I tried to
	 * maintain the same order stuff is moved as in the
	 * real playlist */
	if (dest == 0) {
		list->firstNode->prevNode = srcNode;
		srcNode->nextNode = list->firstNode;
		srcNode->prevNode = NULL;
		list->firstNode = srcNode;
	} else if ((dest + 1) == list->numberOfNodes) {
		list->lastNode->nextNode = srcNode;
		srcNode->nextNode = NULL;
		srcNode->prevNode = list->lastNode;
		list->lastNode = srcNode;
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

int moveSongInStoredPlaylistByPath(int fd, const char *utf8path,
                                   int src, int dest)
{
	List *list;

	if (!(list = loadStoredPlaylist(fd, utf8path))) {
		commandError(fd, ACK_ERROR_UNKNOWN, "could not open playlist");
		return -1;
	}

	if (moveSongInStoredPlaylist(fd, list, src, dest) != 0) {
		freeList(list);
		return -1;
	}

	if (writeStoredPlaylistToPath(fd, list, utf8path) != 0) {
		commandError(fd, ACK_ERROR_UNKNOWN, "failed to save playlist");
		freeList(list);
		return -1;
	}

	freeList(list);
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

static int removeOneSongFromStoredPlaylist(int fd, List *list, int pos)
{
	ListNode *node = nodeOfStoredPlaylist(list, pos);
	if (!node) {
		commandError(fd, ACK_ERROR_ARG,
		             "could not find song at position");
		return -1;
	}

	deleteNodeFromList(list, node);

	return 0;
}

int removeOneSongFromStoredPlaylistByPath(int fd, const char *utf8path, int pos)
{
	List *list;

	if (!(list = loadStoredPlaylist(fd, utf8path))) {
		commandError(fd, ACK_ERROR_UNKNOWN, "could not open playlist");
		return -1;
	}

	if (removeOneSongFromStoredPlaylist(fd, list, pos) != 0) {
		freeList(list);
		return -1;
	}

	if (writeStoredPlaylistToPath(fd, list, utf8path) != 0) {
		commandError(fd, ACK_ERROR_UNKNOWN, "failed to save playlist");
		freeList(list);
		return -1;
	}

	freeList(list);
	return 0;
}

int appendSongToStoredPlaylistByPath(int fd, const char *utf8path, Song *song)
{
	FILE *file;
	char *s;
	struct stat st;
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
	if (fstat(fileno(file), &st) < 0) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not stat file "
		             "\"%s\": %s", path_max_tmp, strerror(errno));
		while (fclose(file) != 0 && errno == EINTR);
		return -1;
	}
	if (st.st_size >= ((MPD_PATH_MAX+1) * playlist_max_length)) {
		while (fclose(file) != 0 && errno == EINTR);
		commandError(fd, ACK_ERROR_PLAYLIST_MAX,
		             "playlist is at the max size");
		return -1;
	}

	s = utf8_to_fs_charset(path_max_tmp2, get_song_url(path_max_tmp, song));

	if (playlist_saveAbsolutePaths && song->type == SONG_TYPE_FILE)
		s = rmp2amp_r(path_max_tmp, s);

	fprintf(file, "%s\n", s);

	while (fclose(file) != 0 && errno == EINTR);
	return 0;
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
