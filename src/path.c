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
#include "playlist.h"
#include "os_compat.h"

#ifdef HAVE_LOCALE
#ifdef HAVE_LANGINFO_CODESET
#include <locale.h>
#include <langinfo.h>
#endif
#endif

const char *musicDir;
static const char *playlistDir;
static size_t music_dir_len;
static size_t playlist_dir_len;
static char *fsCharset;

static char *path_conv_charset(char *dest, const char *to,
			       const char *from, const char *str)
{
	return setCharSetConversion(to, from) ? NULL : char_conv_str(dest, str);
}

char *fs_charset_to_utf8(char *dst, const char *str)
{
	char *ret = path_conv_charset(dst, "UTF-8", fsCharset, str);
	return (ret && !validUtf8String(ret, strlen(ret))) ? NULL : ret;
}

char *utf8_to_fs_charset(char *dst, const char *str)
{
	char *ret = path_conv_charset(dst, fsCharset, "UTF-8", str);
	return ret ? ret : strcpy(dst, str);
}

void setFsCharset(const char *charset)
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

const char *getFsCharset(void)
{
	return fsCharset;
}

void initPaths(void)
{
	ConfigParam *musicParam = parseConfigFilePath(CONF_MUSIC_DIR, 1);
	ConfigParam *playlistParam = parseConfigFilePath(CONF_PLAYLIST_DIR, 1);
	ConfigParam *fsCharsetParam = getConfigParam(CONF_FS_CHARSET);

	char *charset = NULL;
	char *originalLocale;
	DIR *dir;

	musicDir = xstrdup(musicParam->value);
	playlistDir = xstrdup(playlistParam->value);

	music_dir_len = strlen(musicDir);
	playlist_dir_len = strlen(playlistDir);

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

char *pfx_dir(char *dst,
              const char *path, const size_t path_len,
              const char *pfx, const size_t pfx_len)
{
	if (mpd_unlikely((pfx_len + path_len + 1) >= MPD_PATH_MAX))
		FATAL("Cannot prefix '%s' to '%s', PATH_MAX: %d\n",
		      pfx, path, MPD_PATH_MAX);

	/* memmove allows dst == path */
	memmove(dst + pfx_len + 1, path, path_len + 1);
	memcpy(dst, pfx, pfx_len);
	dst[pfx_len] = '/';

	/* this is weird, but directory.c can use it more safely/efficiently */
	return (dst + pfx_len + 1);
}

char *rmp2amp_r(char *dst, const char *rel_path)
{
	pfx_dir(dst, rel_path, strlen(rel_path),
	        (const char *)musicDir, music_dir_len);
	return dst;
}

char *rpp2app_r(char *dst, const char *rel_path)
{
	pfx_dir(dst, rel_path, strlen(rel_path),
	        (const char *)playlistDir, playlist_dir_len);
	return dst;
}

/* this is actually like strlcpy (OpenBSD), but we don't actually want to
 * blindly use it everywhere, only for paths that are OK to truncate (for
 * error reporting and such */
void pathcpy_trunc(char *dest, const char *src)
{
	size_t len = strlen(src);

	if (mpd_unlikely(len >= MPD_PATH_MAX))
		len = MPD_PATH_MAX - 1;
	memcpy(dest, src, len);
	dest[len] = '\0';
}

char *parent_path(char *path_max_tmp, const char *path)
{
	char *c;
	static const int handle_trailing_slashes = 0;

	pathcpy_trunc(path_max_tmp, path);

	if (handle_trailing_slashes) {
		size_t last_char = strlen(path_max_tmp) - 1;

		while (last_char > 0 && path_max_tmp[last_char] == '/')
			path_max_tmp[last_char--] = '\0';
	}

	c = strrchr(path_max_tmp,'/');

	if (c == NULL)
		path_max_tmp[0] = '\0';
	else {
		/* strip redundant slashes: */
		while ((path_max_tmp <= c) && *(--c) == '/')	/* nothing */
			;
		c[1] = '\0';
	}

	return path_max_tmp;
}

char *sanitizePathDup(const char *path)
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

void utf8_to_fs_playlist_path(char *path_max_tmp, const char *utf8path)
{
	utf8_to_fs_charset(path_max_tmp, utf8path);
	rpp2app_r(path_max_tmp, path_max_tmp);
	strncat(path_max_tmp, "." PLAYLIST_FILE_SUFFIX, MPD_PATH_MAX - 1);
}

/* Only takes sanitized paths w/o trailing slashes */
const char *mpd_basename(const char *path)
{
	const char *ret = strrchr(path, '/');

	if (!ret)
		return path;
	++ret;
	assert(*ret != '\0');
	return ret;
}
