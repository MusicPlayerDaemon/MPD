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
#include "client.h"
#include "log.h"
#include "utf8.h"
#include "utils.h"
#include "os_compat.h"

static const char *remoteUrlPrefixes[] = {
	"http://",
	NULL
};

int printRemoteUrlHandlers(struct client *client)
{
	const char **prefixes = remoteUrlPrefixes;

	while (*prefixes) {
		client_printf(client, "handler: %s\n", *prefixes);
		prefixes++;
	}

	return 0;
}

int isValidRemoteUtf8Url(const char *utf8url)
{
	int ret = 0;
	const char *temp;

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

int isRemoteUrl(const char *url)
{
	int count = 0;
	const char **urlPrefixes = remoteUrlPrefixes;

	while (*urlPrefixes) {
		count++;
		if (strncmp(*urlPrefixes, url, strlen(*urlPrefixes)) == 0) {
			return count;
		}
		urlPrefixes++;
	}

	return 0;
}

int lsPlaylists(struct client *client, const char *utf8path)
{
	DIR *dir;
	struct stat st;
	struct dirent *ent;
	char *duplicated;
	char *utf8;
	char s[MPD_PATH_MAX];
	char path_max_tmp[MPD_PATH_MAX];
	List *list = NULL;
	ListNode *node;
	char *actualPath = rpp2app_r(path_max_tmp,
	                             utf8_to_fs_charset(path_max_tmp,
				                        utf8path));
	size_t actlen = strlen(actualPath) + 1;
	size_t maxlen = MPD_PATH_MAX - actlen;
	size_t suflen = strlen(PLAYLIST_FILE_SUFFIX) + 1;
	ssize_t suff;

	if (actlen > MPD_PATH_MAX - 1 || (dir = opendir(actualPath)) == NULL) {
		return 0;
	}

	s[MPD_PATH_MAX - 1] = '\0';
	/* this is safe, notice actlen > MPD_PATH_MAX-1 above */
	strcpy(s, actualPath);
	strcat(s, "/");

	while ((ent = readdir(dir))) {
		size_t len = strlen(ent->d_name) + 1;
		duplicated = ent->d_name;
		if (mpd_likely(len <= maxlen) &&
		    duplicated[0] != '.' &&
		    (suff = (ssize_t)(strlen(duplicated) - suflen)) > 0 &&
		    duplicated[suff] == '.' &&
		    strcmp(duplicated + suff + 1, PLAYLIST_FILE_SUFFIX) == 0) {
			memcpy(s + actlen, ent->d_name, len);
			if (stat(s, &st) == 0) {
				if (S_ISREG(st.st_mode)) {
					if (list == NULL)
						list = makeList(NULL, 1);
					duplicated[suff] = '\0';
					utf8 = fs_charset_to_utf8(path_max_tmp,
					                          duplicated);
					if (utf8)
						insertInList(list, utf8, NULL);
				}
			}
		}
	}

	closedir(dir);

	if (list) {
		int i;
		sortList(list);

		duplicated = xmalloc(strlen(utf8path) + 2);
		strcpy(duplicated, utf8path);
		for (i = strlen(duplicated) - 1;
		     i >= 0 && duplicated[i] == '/';
		     i--) {
			duplicated[i] = '\0';
		}
		if (strlen(duplicated))
			strcat(duplicated, "/");

		node = list->firstNode;
		while (node != NULL) {
			if (!strchr(node->key, '\n')) {
				client_printf(client, "playlist: %s%s\n",
					      duplicated, node->key);
			}
			node = node->nextNode;
		}

		freeList(list);
		free(duplicated);
	}

	return 0;
}

int myStat(const char *utf8file, struct stat *st)
{
	char path_max_tmp[MPD_PATH_MAX];
	const char *file = utf8_to_fs_charset(path_max_tmp, utf8file);
	const char *actualFile = file;

	if (actualFile[0] != '/')
		actualFile = rmp2amp_r(path_max_tmp, file);

	return stat(actualFile, st);
}

int isFile(const char *utf8file, time_t * mtime)
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
const char *getSuffix(const char *utf8file)
{
	const char *ret = NULL;

	while (*utf8file) {
		if (*utf8file == '.')
			ret = utf8file + 1;
		utf8file++;
	}

	return ret;
}

static int hasSuffix(const char *utf8file, const char *suffix)
{
	const char *s = getSuffix(utf8file);
	if (s && 0 == strcmp(s, suffix))
		return 1;
	return 0;
}

int isPlaylist(const char *utf8file)
{
	if (isFile(utf8file, NULL)) {
		return hasSuffix(utf8file, PLAYLIST_FILE_SUFFIX);
	}
	return 0;
}

int isDir(const char *utf8name)
{
	struct stat st;

	if (myStat(utf8name, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			return 1;
		}
	}

	return 0;
}

struct decoder_plugin *hasMusicSuffix(const char *utf8file, unsigned int next)
{
	struct decoder_plugin *ret = NULL;

	const char *s = getSuffix(utf8file);
	if (s) {
		ret = decoder_plugin_from_suffix(s, next);
	} else {
		DEBUG("hasMusicSuffix: The file: %s has no valid suffix\n",
		      utf8file);
	}

	return ret;
}

struct decoder_plugin *isMusic(const char *utf8file, time_t * mtime,
			       unsigned int next)
{
	if (isFile(utf8file, mtime)) {
		struct decoder_plugin *plugin = hasMusicSuffix(utf8file, next);
		if (plugin != NULL)
			return plugin;
	}
	DEBUG("isMusic: %s is not a valid file\n", utf8file);
	return NULL;
}
