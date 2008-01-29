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

static char *utf8pathToFsPathInStoredPlaylist(const char *utf8path, int fd)
{
	char *file;
	char *rfile;
	char *actualFile;

	if (strstr(utf8path, "/")) {
		commandError(fd, ACK_ERROR_ARG, "playlist name \"%s\" is "
		             "invalid: playlist names may not contain slashes",
		             utf8path);
		return NULL;
	}

	file = utf8ToFsCharset((char *)utf8path);

	rfile = xmalloc(strlen(file) + strlen(".") +
	                strlen(PLAYLIST_FILE_SUFFIX) + 1);

	strcpy(rfile, file);
	strcat(rfile, ".");
	strcat(rfile, PLAYLIST_FILE_SUFFIX);

	actualFile = rpp2app(rfile);

	free(rfile);

	return actualFile;
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
	insertInListWithoutKey(sp->list, xstrdup(getSongUrl(song)));
}

StoredPlaylist *newStoredPlaylist(const char *utf8name, int fd, int ignoreExisting)
{
	struct stat buf;
	char *filename = NULL;
	StoredPlaylist *sp = calloc(1, sizeof(*sp));
	if (!sp)
		return NULL;

	if (utf8name) {
		filename = utf8pathToFsPathInStoredPlaylist(utf8name, fd);

		if (filename && stat(filename, &buf) == 0 &&
		    ignoreExisting == 0) {
			commandError(fd, ACK_ERROR_EXIST,
			             "a file or directory already exists with "
			             "the name \"%s\"", utf8name);
			free(sp);
			return NULL;
		}
	}

	sp->list = makeList(DEFAULT_FREE_DATA_FUNC, 0);
	sp->fd = fd;

	if (filename)
		sp->fspath = xstrdup(filename);

	return sp;
}

StoredPlaylist *loadStoredPlaylist(const char *utf8path, int fd)
{
	char *filename;
	StoredPlaylist *sp;
	FILE *file;
	char s[MAXPATHLEN + 1];
	int slength = 0;
	char *temp = utf8ToFsCharset((char *)utf8path);
	char *parent = parentPath(temp);
	int parentlen = strlen(parent);
	int tempInt;
	int commentCharFound = 0;
	Song *song;

	filename = utf8pathToFsPathInStoredPlaylist(utf8path, fd);
	if (!filename)
		return NULL;

	while (!(file = fopen(filename, "r")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", filename, strerror(errno));
		return NULL;
	}

	sp = newStoredPlaylist(utf8path, fd, 1);
	if (!sp)
		goto out;

	while ((tempInt = fgetc(file)) != EOF) {
		s[slength] = tempInt;
		if (s[slength] == '\n' || s[slength] == '\0') {
			commentCharFound = 0;
			s[slength] = '\0';
			if (s[0] == PLAYLIST_COMMENT)
				commentCharFound = 1;
			if (strncmp(s, musicDir, strlen(musicDir)) == 0) {
				strcpy(s, &(s[strlen(musicDir)]));
			} else if (parentlen) {
				temp = xstrdup(s);
				memset(s, 0, MAXPATHLEN + 1);
				strcpy(s, parent);
				strncat(s, "/", MAXPATHLEN - parentlen);
				strncat(s, temp, MAXPATHLEN - parentlen - 1);
				if (strlen(s) >= MAXPATHLEN) {
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
			temp = fsCharsetToUtf8(s);
			if (temp && !commentCharFound) {
				if (sp->list->numberOfNodes >=
				    (playlist_max_length - 1))
					goto out;
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
		} else if (slength == MAXPATHLEN) {
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

/* Not used currently
static void removeAllFromStoredPlaylist(StoredPlaylist *sp)
{
	freeList(sp->list);
	sp->list = makeList(DEFAULT_FREE_DATA_FUNC, 0);
}
*/

int removeAllFromStoredPlaylistByPath(int fd, const char *utf8path)
{
	char *filename;
	FILE *file;

	filename = utf8pathToFsPathInStoredPlaylist(utf8path, fd);
	if (!filename)
		return -1;

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
		s = (char *)node->data;
		if (isValidRemoteUtf8Url(s) || !playlist_saveAbsolutePaths)
			s = utf8ToFsCharset(s);
		else
			s = rmp2amp(utf8ToFsCharset(s));
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
	char *filename;
	FILE *file;
	char *s;
	struct stat st;
	char path_max_tmp[MAXPATHLEN + 1];

	filename = utf8pathToFsPathInStoredPlaylist(utf8path, fd);
	if (!filename)
		return -1;

	while (!(file = fopen(filename, "a")) && errno == EINTR);
	if (file == NULL) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not open file "
		             "\"%s\": %s", filename, strerror(errno));
		return -1;
	}
	if (fstat(fileno(file), &st) < 0) {
		commandError(fd, ACK_ERROR_NO_EXIST, "could not stat file "
		             "\"%s\": %s", path_max_tmp, strerror(errno));
		return -1;
	}
	if (st.st_size >= ((MAXPATHLEN+1) * playlist_max_length)) {
		commandError(fd, ACK_ERROR_PLAYLIST_MAX,
		             "playlist is at the max size");
		return -1;
	}

	if (playlist_saveAbsolutePaths && song->type == SONG_TYPE_FILE)
		s = rmp2amp(utf8ToFsCharset(getSongUrl(song)));
	else
		s = utf8ToFsCharset(getSongUrl(song));

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
	char *from;
	char *to;
	int ret = 0;

	from = utf8pathToFsPathInStoredPlaylist(utf8from, fd);
	if (!from)
		return -1;
	from = xstrdup(from);
	if (!from)
		return -1;

	to = utf8pathToFsPathInStoredPlaylist(utf8to, fd);
	if (!to) {
		free(from);
		return -1;
	}
	to = xstrdup(to);
	if (!to) {
		free(from);
		return -1;
	}

	if (stat(from, &st) != 0) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "no playlist named \"%s\"", utf8from);
		ret = -1;
		goto out;
	}

	if (stat(to, &st) == 0) {
		commandError(fd, ACK_ERROR_EXIST, "a file or directory "
			     "already exists with the name \"%s\"", utf8to);
		ret = -1;
		goto out;
	}

	if (rename(from, to) < 0) {
		commandError(fd, ACK_ERROR_UNKNOWN,
		             "could not rename playlist \"%s\" to \"%s\": %s",
		             utf8from, utf8to, strerror(errno));
		ret = -1;
		goto out;
	}

out:
	free(from);
	free(to);

	return ret;
}
