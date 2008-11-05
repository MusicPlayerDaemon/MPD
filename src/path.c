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
#include "conf.h"
#include "utils.h"
#include "playlist.h"
#include "os_compat.h"

#include <glib.h>

static char *fs_charset;

char *fs_charset_to_utf8(char *dst, const char *str)
{
	gchar *p;
	GError *error = NULL;

	p = g_convert(str, -1,
		      fs_charset, "utf-8",
		      NULL, NULL, &error);
	if (p == NULL) {
		/* no fallback */
		g_error_free(error);
		return NULL;
	}

	g_strlcpy(dst, p, MPD_PATH_MAX);
	g_free(p);
	return dst;
}

char *utf8_to_fs_charset(char *dst, const char *str)
{
	gchar *p;
	GError *error = NULL;

	p = g_convert(str, -1,
		      "utf-8", fs_charset,
		      NULL, NULL, &error);
	if (p == NULL) {
		/* fall back to UTF-8 */
		g_error_free(error);
		return strcpy(dst, str);
	}

	g_strlcpy(dst, p, MPD_PATH_MAX);
	g_free(p);
	return dst;
}

void path_set_fs_charset(const char *charset)
{
	int error = 0;

	g_free(fs_charset);
	fs_charset = g_strdup(charset);

	DEBUG("path_set_fs_charset: fs charset is: %s\n", fs_charset);

	if (error) {
		free(fs_charset);
		WARNING("setting fs charset to ISO-8859-1!\n");
		fs_charset = xstrdup("ISO-8859-1");
	}
}

const char *path_get_fs_charset(void)
{
	return fs_charset;
}

void path_global_init(void)
{
	ConfigParam *fs_charset_param = getConfigParam(CONF_FS_CHARSET);

	char *charset = NULL;

	if (fs_charset_param) {
		charset = xstrdup(fs_charset_param->value);
	} else {
		const gchar **encodings;
		g_get_filename_charsets(&encodings);

		if (encodings[0] != NULL && *encodings[0] != '\0')
			charset = g_strdup(encodings[0]);
	}

	if (charset) {
		path_set_fs_charset(charset);
		free(charset);
	} else {
		WARNING("setting filesystem charset to ISO-8859-1\n");
		path_set_fs_charset("ISO-8859-1");
	}
}

void path_global_finish(void)
{
	g_free(fs_charset);
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
