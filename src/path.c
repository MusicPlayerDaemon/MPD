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

#include "path.h"
#include "log.h"
#include "charConv.h"
#include "conf.h"
#include "utf8.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
#include <locale.h>
#include <langinfo.h>
#endif
#endif

const char *musicDir;
static const char *playlistDir;
static char *fsCharset;

static char *pathConvCharset(char *to, char *from, char *str, char *ret)
{
	if (ret)
		free(ret);
	return setCharSetConversion(to, from) ? NULL : convStrDup(str);
}

char *fsCharsetToUtf8(char *str)
{
	static char *ret;

	ret = pathConvCharset("UTF-8", fsCharset, str, ret);

	if (ret && !validUtf8String(ret)) {
		free(ret);
		ret = NULL;
	}

	return ret;
}

char *utf8ToFsCharset(char *str)
{
	static char *ret;

	ret = pathConvCharset(fsCharset, "UTF-8", str, ret);

	if (!ret)
		ret = xstrdup(str);

	return ret;
}

void setFsCharset(char *charset)
{
	int error = 0;

	if (fsCharset)
		free(fsCharset);

	fsCharset = xstrdup(charset);

	DEBUG("setFsCharset: fs charset is: %s\n", fsCharset);

	if (setCharSetConversion("UTF-8", fsCharset) != 0) {
		WARNING("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			fsCharset, "UTF-8");
		error = 1;
	}
	if (setCharSetConversion(fsCharset, "UTF-8") != 0) {
		WARNING("fs charset conversion problem: "
			"not able to convert from \"%s\" to \"%s\"\n",
			"UTF-8", fsCharset);
		error = 1;
	}

	if (error) {
		free(fsCharset);
		WARNING("setting fs charset to ISO-8859-1!\n");
		fsCharset = xstrdup("ISO-8859-1");
	}
}

char *getFsCharset(void)
{
	return fsCharset;
}

static char *appendSlash(char **path)
{
	char *temp = *path;
	int len = strlen(temp);

	if (temp[len - 1] != '/') {
		temp = xmalloc(len + 2);
		memset(temp, 0, len + 2);
		memcpy(temp, *path, len);
		temp[len] = '/';
		free(*path);
		*path = temp;
	}

	return temp;
}

void initPaths(void)
{
	ConfigParam *musicParam = parseConfigFilePath(CONF_MUSIC_DIR, 1);
	ConfigParam *playlistParam = parseConfigFilePath(CONF_PLAYLIST_DIR, 1);
	ConfigParam *fsCharsetParam = getConfigParam(CONF_FS_CHARSET);

	char *charset = NULL;
	char *originalLocale;
	DIR *dir;

	musicDir = appendSlash(&(musicParam->value));
	playlistDir = appendSlash(&(playlistParam->value));

	if ((dir = opendir(playlistDir)) == NULL) {
		FATAL("cannot open %s \"%s\" (config line %i): %s\n",
		      CONF_PLAYLIST_DIR, playlistParam->value,
		      playlistParam->line, strerror(errno));
	}
	closedir(dir);

	if ((dir = opendir(musicDir)) == NULL) {
		FATAL("cannot open %s \"%s\" (config line %i): %s\n",
		      CONF_MUSIC_DIR, musicParam->value,
		      musicParam->line, strerror(errno));
	}
	closedir(dir);

	if (fsCharsetParam) {
		charset = xstrdup(fsCharsetParam->value);
	}
#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
	else if ((originalLocale = setlocale(LC_CTYPE, NULL))) {
		char *temp;
		char *currentLocale;
		originalLocale = xstrdup(originalLocale);

		if (!(currentLocale = setlocale(LC_CTYPE, ""))) {
			WARNING("problems setting current locale with "
				"setlocale()\n");
		} else {
			if (strcmp(currentLocale, "C") == 0 ||
			    strcmp(currentLocale, "POSIX") == 0) {
				WARNING("current locale is \"%s\"\n",
					currentLocale);
			} else if ((temp = nl_langinfo(CODESET))) {
				charset = xstrdup(temp);
			} else
				WARNING
				    ("problems getting charset for locale\n");
			if (!setlocale(LC_CTYPE, originalLocale)) {
				WARNING
				    ("problems resetting locale with setlocale()\n");
			}
		}

		free(originalLocale);
	} else
		WARNING("problems getting locale with setlocale()\n");
#endif
#endif

	if (charset) {
		setFsCharset(charset);
		free(charset);
	} else {
		WARNING("setting filesystem charset to ISO-8859-1\n");
		setFsCharset("ISO-8859-1");
	}
}

void finishPaths(void)
{
	free(fsCharset);
	fsCharset = NULL;
}

static char *pfx_path(const char *path, const char *pfx, const size_t pfx_len)
{
	static char ret[MAXPATHLEN+1];
	size_t rp_len = strlen(path);

	/* check for the likely condition first: */
	if (mpd_likely((pfx_len + rp_len) < MAXPATHLEN)) {
		memcpy(ret, pfx, pfx_len);
		memcpy(ret + pfx_len, path, rp_len + 1);
		return ret;
	}

	/* unlikely, return an empty string because truncating would
	 * also be wrong... break early and break loudly (the system
	 * headers are likely screwed, not mpd) */
	ERROR("Cannot prefix '%s' to '%s', max: %d", pfx, path, MAXPATHLEN);
	ret[0] = '\0';
	return ret;
}

char *rmp2amp(char *relativePath)
{
	size_t pfx_len = strlen(musicDir);
	return pfx_path(relativePath, musicDir, pfx_len);
}

char *rpp2app(char *relativePath)
{
	size_t pfx_len = strlen(playlistDir);
	return pfx_path(relativePath, playlistDir, pfx_len);
}

/* this is actually like strlcpy (OpenBSD), but we don't actually want to
 * blindly use it everywhere, only for paths that are OK to truncate (for
 * error reporting and such */
void pathcpy_trunc(char *dest, const char *src)
{
	size_t len = strlen(src);

	if (mpd_unlikely(len > MAXPATHLEN))
		len = MAXPATHLEN;
	memcpy(dest, src, len);
	dest[len] = '\0';
}

char *parentPath(char *path)
{
	static char parentPath[MAXPATHLEN+1];
	char *c;

	pathcpy_trunc(parentPath, path);
	c = strrchr(parentPath,'/');

	if (c == NULL)
		parentPath[0] = '\0';
	else {
		while ((parentPath <= c) && *(--c) == '/')	/* nothing */
			;
		c[1] = '\0';
	}

	return parentPath;
}

char *sanitizePathDup(char *path)
{
	int len = strlen(path) + 1;
	char *ret = xmalloc(len);
	char *cp = ret;

	memset(ret, 0, len);

	len = 0;

	/* eliminate more than one '/' in a row, like "///" */
	while (*path) {
		while (*path == '/')
			path++;
		if (*path == '.') {
			/* we don't want to have hidden directories, or '.' or
			   ".." in our path */
			free(ret);
			return NULL;
		}
		while (*path && *path != '/') {
			*(cp++) = *(path++);
			len++;
		}
		if (*path == '/') {
			*(cp++) = *(path++);
			len++;
		}
	}

	if (len && ret[len - 1] == '/') {
		len--;
		ret[len] = '\0';
	}

	DEBUG("sanitized: %s\n", ret);

	return xrealloc(ret, len + 1);
}
