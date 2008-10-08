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
#include "song.h"
#include "path.h"
#include "utils.h"
#include "ls.h"
#include "database.h"
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

static enum playlist_result
writeStoredPlaylistToPath(List *list, const char *utf8path)
{
	ListNode *node;
	FILE *file;
	char *s;
	char path_max_tmp[MPD_PATH_MAX];

	assert(utf8path != NULL);

	utf8_to_fs_playlist_path(path_max_tmp, utf8path);

	while (!(file = fopen(path_max_tmp, "w")) && errno == EINTR);
	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	node = list->firstNode;
	while (node != NULL) {
		s = utf8_to_fs_charset(path_max_tmp, (char *)node->data);
		if (playlist_saveAbsolutePaths && !isValidRemoteUtf8Url(s))
			s = rmp2amp_r(path_max_tmp, s);
		fprintf(file, "%s\n", s);
		node = node->nextNode;
	}

	while (fclose(file) != 0 && errno == EINTR);
	return PLAYLIST_RESULT_SUCCESS;
}

List *loadStoredPlaylist(const char *utf8path)
{
	List *list;
	FILE *file;
	char buffer[MPD_PATH_MAX];
	char path_max_tmp[MPD_PATH_MAX];
	const size_t musicDir_len = strlen(musicDir);

	if (!is_valid_playlist_name(utf8path))
		return NULL;

	utf8_to_fs_playlist_path(path_max_tmp, utf8path);
	while (!(file = fopen(path_max_tmp, "r")) && errno == EINTR);
	if (file == NULL)
		return NULL;

	list = makeList(DEFAULT_FREE_DATA_FUNC, 0);

	while (myFgets(buffer, sizeof(buffer), file)) {
		char *s = buffer;
		struct song *song;

		if (*s == PLAYLIST_COMMENT)
			continue;
		if (s[musicDir_len] == '/' &&
		    !strncmp(s, musicDir, musicDir_len))
			memmove(s, s + musicDir_len + 1,
				strlen(s + musicDir_len + 1) + 1);
		if ((song = getSongFromDB(s))) {
			song_get_url(song, path_max_tmp);
			insertInListWithoutKey(list, xstrdup(path_max_tmp));
		} else if (isValidRemoteUtf8Url(s))
			insertInListWithoutKey(list, xstrdup(s));

		if (list->numberOfNodes >= playlist_max_length)
			break;
	}

	while (fclose(file) && errno == EINTR);
	return list;
}

static int moveSongInStoredPlaylist(List *list, int src, int dest)
{
	ListNode *srcNode, *destNode;

	if (src >= list->numberOfNodes || dest >= list->numberOfNodes ||
	    src < 0 || dest < 0 || src == dest)
		return -1;

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

enum playlist_result
moveSongInStoredPlaylistByPath(const char *utf8path, int src, int dest)
{
	List *list;
	enum playlist_result result;

	if (!(list = loadStoredPlaylist(utf8path)))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (moveSongInStoredPlaylist(list, src, dest) != 0) {
		freeList(list);
		return PLAYLIST_RESULT_BAD_RANGE;
	}

	result = writeStoredPlaylistToPath(list, utf8path);

	freeList(list);
	return result;
}

enum playlist_result
removeAllFromStoredPlaylistByPath(const char *utf8path)
{
	char filename[MPD_PATH_MAX];
	FILE *file;

	if (!is_valid_playlist_name(utf8path))
		return PLAYLIST_RESULT_BAD_NAME;

	utf8_to_fs_playlist_path(filename, utf8path);

	while (!(file = fopen(filename, "w")) && errno == EINTR);
	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	while (fclose(file) != 0 && errno == EINTR);
	return PLAYLIST_RESULT_SUCCESS;
}

static int removeOneSongFromStoredPlaylist(List *list, int pos)
{
	ListNode *node = nodeOfStoredPlaylist(list, pos);
	if (!node)
		return -1;

	deleteNodeFromList(list, node);

	return 0;
}

enum playlist_result
removeOneSongFromStoredPlaylistByPath(const char *utf8path, int pos)
{
	List *list;
	enum playlist_result result;

	if (!(list = loadStoredPlaylist(utf8path)))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (removeOneSongFromStoredPlaylist(list, pos) != 0) {
		freeList(list);
		return PLAYLIST_RESULT_BAD_RANGE;
	}

	result = writeStoredPlaylistToPath(list, utf8path);

	freeList(list);
	return result;
}

enum playlist_result
appendSongToStoredPlaylistByPath(const char *utf8path, struct song *song)
{
	FILE *file;
	char *s;
	struct stat st;
	char path_max_tmp[MPD_PATH_MAX];
	char path_max_tmp2[MPD_PATH_MAX];

	if (!is_valid_playlist_name(utf8path))
		return PLAYLIST_RESULT_BAD_NAME;
	utf8_to_fs_playlist_path(path_max_tmp, utf8path);

	while (!(file = fopen(path_max_tmp, "a")) && errno == EINTR);
	if (file == NULL) {
		int save_errno = errno;
		while (fclose(file) != 0 && errno == EINTR);
		errno = save_errno;
		return PLAYLIST_RESULT_ERRNO;
	}

	if (fstat(fileno(file), &st) < 0) {
		int save_errno = errno;
		while (fclose(file) != 0 && errno == EINTR);
		errno = save_errno;
		return PLAYLIST_RESULT_ERRNO;
	}

	if (st.st_size >= ((MPD_PATH_MAX+1) * playlist_max_length)) {
		while (fclose(file) != 0 && errno == EINTR);
		return PLAYLIST_RESULT_TOO_LARGE;
	}

	s = utf8_to_fs_charset(path_max_tmp2,
			       song_get_url(song, path_max_tmp));

	if (playlist_saveAbsolutePaths && song_is_file(song))
		s = rmp2amp_r(path_max_tmp, s);

	fprintf(file, "%s\n", s);

	while (fclose(file) != 0 && errno == EINTR);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
renameStoredPlaylist(const char *utf8from, const char *utf8to)
{
	struct stat st;
	char from[MPD_PATH_MAX];
	char to[MPD_PATH_MAX];

	if (!is_valid_playlist_name(utf8from) ||
	    !is_valid_playlist_name(utf8to))
		return PLAYLIST_RESULT_BAD_NAME;

	utf8_to_fs_playlist_path(from, utf8from);
	utf8_to_fs_playlist_path(to, utf8to);

	if (stat(from, &st) != 0)
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (stat(to, &st) == 0)
		return PLAYLIST_RESULT_LIST_EXISTS;

	if (rename(from, to) < 0)
		return PLAYLIST_RESULT_ERRNO;

	return PLAYLIST_RESULT_SUCCESS;
}
