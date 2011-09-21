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

#ifndef MPD_URI_H
#define MPD_URI_H

#include <glib.h>

#include <stdbool.h>

/**
 * Checks whether the specified URI has a scheme in the form
 * "scheme://".
 */
G_GNUC_PURE
bool uri_has_scheme(const char *uri);

G_GNUC_PURE
const char *
uri_get_suffix(const char *uri);

/**
 * Returns true if this is a safe "local" URI:
 *
 * - non-empty
 * - does not begin or end with a slash
 * - no double slashes
 * - no path component begins with a dot
 */
G_GNUC_PURE
bool
uri_safe_local(const char *uri);

/**
 * Removes HTTP username and password from the URI.  This may be
 * useful for displaying an URI without disclosing secrets.  Returns
 * NULL if nothing needs to be removed, or if the URI is not
 * recognized.
 */
G_GNUC_MALLOC
char *
uri_remove_auth(const char *uri);

#endif
