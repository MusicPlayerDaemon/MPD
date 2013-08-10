/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#if !GLIB_CHECK_VERSION(2,28,0)

static inline gint64
g_source_get_time(GSource *source)
{
	GTimeVal tv;
	g_source_get_current_time(source, &tv);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

#endif

#endif
