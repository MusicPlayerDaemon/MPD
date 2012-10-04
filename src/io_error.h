/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_IO_ERROR_H
#define MPD_IO_ERROR_H

#include <glib.h>

#include <errno.h>

/**
 * A GQuark for GError for I/O errors.  The code is an errno value.
 */
G_GNUC_CONST
static inline GQuark
errno_quark(void)
{
	return g_quark_from_static_string("errno");
}

static inline void
set_error_errno(GError **error_r)
{
	g_set_error_literal(error_r, errno_quark(), errno,
			    g_strerror(errno));
}

#endif
