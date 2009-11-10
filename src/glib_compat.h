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

#endif
