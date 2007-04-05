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

#include "ls.h"
#include "playlist.h"
#include "path.h"
#include "myfprintf.h"
#include "log.h"
#include "utf8.h"
#include "utils.h"

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

static char *remoteUrlPrefixes[] = {
	"http://",
	NULL
};

int printRemoteUrlHandlers(int fd)
{
	char **prefixes = remoteUrlPrefixes;

	while (*prefixes) {
		fdprintf(fd, "handler: %s\n", *prefixes);
		prefixes++;
	}

	return 0;
}

int isValidRemoteUtf8Url(char *utf8url)
{
	int ret = 0;
	char *temp;

	switch (isRemoteUrl(utf8url)) {
	case 1:
		ret = 1;
		temp = utf8url;
		while (*temp) {
			if ((*temp >= 'a' && *temp <= 'z') ||
			    (*temp >= 'A' && *temp <= 'Z') ||
			    (*temp >= '0' && *temp <= '9') ||
			    *temp == '$' ||
			    *temp == '-' ||
			    *temp == '.' ||
			    *temp == '+' ||
			    *temp == '!' ||
			    *temp == '*' ||
			    *temp == '\'' ||
			    *temp == '(' ||
			    *temp == ')' ||
			    *temp == ',' ||
			    *temp == '%' ||
			    *temp == '/' ||
			    *temp == ':' ||
			    *temp == '?' ||
			    *temp == ';' || *temp == '&' || *temp == '=') {
			} else {
				ret = 1;
				break;
			}
			temp++;
		}
		break;
	}

	return ret;
}

int isRemoteUrl(char *url)
{
	int count = 0;
	char **urlPrefixes = remoteUrlPrefixes;

	while (*urlPrefixes) {
		count++;
		if (strncmp(*urlPrefixes, url, strlen(*urlPrefixes)) == 0) {
			return count;
		}
		urlPrefixes++;
	}

	return 0;
}

int lsPlaylists(int fd, char *utf8path)
{
	DIR *dir;
	struct stat st;
	struct dirent *ent;
	char *dup;
	char *utf8;
	char s[MAXPATHLEN + 1];
	List *list = NULL;
	ListNode *node = NULL;
	char *path = utf8ToFsCharset(utf8path);
	char *actualPath = rpp2app(path);
	int actlen = strlen(actualPath) + 1;
	int maxlen = MAXPATHLEN - actlen;
	int suflen = strlen(PLAYLIST_FILE_SUFFIX) + 1;
	int suff;

	if (actlen > MAXPATHLEN - 1 || (dir = opendir(actualPath)) == NULL) {
		return 0;
	}

	s[MAXPATHLEN] = '\0';
	/* this is safe, notice actlen > MAXPATHLEN-1 above */
	strcpy(s, actualPath);
	strcat(s, "/");

	while ((ent = readdir(dir))) {
		size_t len = strlen(ent->d_name) + 1;
		dup = ent->d_name;
		if (mpd_likely(len <= maxlen) &&
		    dup[0] != '.' &&
		    (suff = strlen(dup) - suflen) > 0 &&
		    dup[suff] == '.' &&
		    strcmp(dup + suff + 1, PLAYLIST_FILE_SUFFIX) == 0) {
			memcpy(s + actlen, ent->d_name, len);
			if (stat(s, &st) == 0) {
				if (S_ISREG(st.st_mode)) {
					if (list == NULL)
						list = makeList(NULL, 1);
					dup[suff] = '\0';
					if ((utf8 = fsCharsetToUtf8(dup))) {
						insertInList(list, utf8, NULL);
					}
				}
			}
		}
	}

	closedir(dir);

	if (list) {
		int i;
		sortList(list);

		dup = xmalloc(strlen(utf8path) + 2);
		strcpy(dup, utf8path);
		for (i = strlen(dup) - 1; i >= 0 && dup[i] == '/'; i--) {
			dup[i] = '\0';
		}
		if (strlen(dup))
			strcat(dup, "/");

		node = list->firstNode;
		while (node != NULL) {
			if (!strchr(node->key, '\n')) {
				fdprintf(fd, "playlist: %s%s\n", dup,
					  node->key);
			}
			node = node->nextNode;
		}

		freeList(list);
		free(dup);
	}

	return 0;
}

int myStat(char *utf8file, struct stat *st)
{
	char *file = utf8ToFsCharset(utf8file);
	char *actualFile = file;

	if (actualFile[0] != '/')
		actualFile = rmp2amp(file);

	return stat(actualFile, st);
}

static int isFile(char *utf8file, time_t * mtime)
{
	struct stat st;

	if (myStat(utf8file, &st) == 0) {
		if (S_ISREG(st.st_mode)) {
			if (mtime)
				*mtime = st.st_mtime;
			return 1;
		} else {
			DEBUG("isFile: %s is not a regular file\n", utf8file);
			return 0;
		}
	} else {
		DEBUG("isFile: failed to stat: %s: %s\n", utf8file,
		      strerror(errno));
	}

	return 0;
}

/* suffixes should be ascii only characters */
char *getSuffix(char *utf8file)
{
	char *ret = NULL;

	while (*utf8file) {
		if (*utf8file == '.')
			ret = utf8file + 1;
		utf8file++;
	}

	return ret;
}

static int hasSuffix(char *utf8file, char *suffix)
{
	char *s = getSuffix(utf8file);
	if (s && 0 == strcmp(s, suffix))
		return 1;
	return 0;
}

int isPlaylist(char *utf8file)
{
	if (isFile(utf8file, NULL)) {
		return hasSuffix(utf8file, PLAYLIST_FILE_SUFFIX);
	}
	return 0;
}

int isDir(char *utf8name)
{
	struct stat st;

	if (myStat(utf8name, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			return 1;
		}
	}

	return 0;
}

InputPlugin *hasMusicSuffix(char *utf8file, unsigned int next)
{
	InputPlugin *ret = NULL;

	char *s = getSuffix(utf8file);
	if (s) {
		ret = getInputPluginFromSuffix(s, next);
	} else {
		DEBUG("hasMusicSuffix: The file: %s has no valid suffix\n",
		      utf8file);
	}

	return ret;
}

InputPlugin *isMusic(char *utf8file, time_t * mtime, unsigned int next)
{
	if (isFile(utf8file, mtime)) {
		InputPlugin *plugin = hasMusicSuffix(utf8file, next);
		if (plugin != NULL)
			return plugin;
	}
	DEBUG("isMusic: %s is not a valid file\n", utf8file);
	return NULL;
}
