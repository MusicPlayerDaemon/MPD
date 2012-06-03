/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

/*
 * Compatibility with older GLib versions.  Some of this isn't
 * implemented properly, just "good enough" to allow users with older
 * operating systems to run MPD.
 */

#ifndef MPD_GLIB_COMPAT_H
#define MPD_GLIB_COMPAT_H

#include <glib.h>

#if !GLIB_CHECK_VERSION(2,14,0)

#define g_queue_clear(q) do { g_queue_free(q); q = g_queue_new(); } while (0)

static inline guint
g_timeout_add_seconds(guint interval, GSourceFunc function, gpointer data)
{
	return g_timeout_add(interval * 1000, function, data);
}

#endif /* !2.14 */

#if !GLIB_CHECK_VERSION(2,16,0)

static inline void
g_propagate_prefixed_error(GError **dest_r, GError *src,
			   G_GNUC_UNUSED const gchar *format, ...)
{
	g_propagate_error(dest_r, src);
}

static inline char *
g_uri_escape_string(const char *unescaped,
		    G_GNUC_UNUSED const char *reserved_chars_allowed,
		    G_GNUC_UNUSED gboolean allow_utf8)
{
	return g_strdup(unescaped);
}

#endif /* !2.16 */

#if !GLIB_CHECK_VERSION(2,16,0)

#include <string.h>

static inline char *
g_uri_parse_scheme(const char *uri)
{
	const char *end = strstr(uri, "://");
	if (end == NULL)
		return NULL;
	return g_strndup(uri, end - uri);
}

#endif

#if defined(G_OS_WIN32) && defined(g_file_test)

/* Modern GLib on Win32 likes to use UTF-8 for file names.
It redefines g_file_test() to be g_file_test_utf8().
This gives incorrect results for non-ASCII files.
Old g_file_test() is available for *binary compatibility*,
but symbol is hidden from linker, we copy-paste its definition here */

#undef g_file_test

static inline gboolean
g_file_test(const gchar *filename, GFileTest test)
{
	gchar *utf8_filename = g_locale_to_utf8(filename, -1, NULL, NULL, NULL);
	gboolean retval;

	if (utf8_filename == NULL)
		return FALSE;

	retval = g_file_test_utf8(utf8_filename, test);

	g_free(utf8_filename);

	return retval;
}

#endif

#endif
