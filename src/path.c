/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "path.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "path"

static char *fs_charset;

char *
fs_charset_to_utf8(const char *path_fs)
{
	return g_convert(path_fs, -1,
			 "utf-8", fs_charset,
			 NULL, NULL, NULL);
}

char *
utf8_to_fs_charset(const char *path_utf8)
{
	gchar *p;

	p = g_convert(path_utf8, -1,
		      fs_charset, "utf-8",
		      NULL, NULL, NULL);
	if (p == NULL)
		/* fall back to UTF-8 */
		p = g_strdup(path_utf8);

	return p;
}

static void
path_set_fs_charset(const char *charset)
{
	char *test;

	assert(charset != NULL);

	/* convert a space to ensure that the charset is valid */
	test = g_convert(" ", 1, charset, "UTF-8", NULL, NULL, NULL);
	if (test == NULL)
		g_error("invalid filesystem charset: %s", charset);
	g_free(test);

	g_free(fs_charset);
	fs_charset = g_strdup(charset);

	g_debug("path_set_fs_charset: fs charset is: %s", fs_charset);
}

const char *path_get_fs_charset(void)
{
	return fs_charset;
}

void path_global_init(void)
{
	const char *charset = NULL;

	charset = config_get_string(CONF_FS_CHARSET, NULL);
	if (charset == NULL) {
		const gchar **encodings;
		g_get_filename_charsets(&encodings);

		if (encodings[0] != NULL && *encodings[0] != '\0')
			charset = encodings[0];
	}

	if (charset) {
		path_set_fs_charset(charset);
	} else {
		g_message("setting filesystem charset to ISO-8859-1");
		path_set_fs_charset("ISO-8859-1");
	}
}

void path_global_finish(void)
{
	g_free(fs_charset);
}
