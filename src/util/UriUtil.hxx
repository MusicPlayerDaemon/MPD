/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_URI_UTIL_HXX
#define MPD_URI_UTIL_HXX

#include "Compiler.h"

#include <string>

/**
 * Checks whether the specified URI has a scheme in the form
 * "scheme://".
 */
gcc_pure
bool uri_has_scheme(const char *uri);

/**
 * Returns the scheme name of the specified URI, or an empty string.
 */
gcc_pure
std::string
uri_get_scheme(const char *uri);

gcc_pure
const char *
uri_get_suffix(const char *uri);

struct UriSuffixBuffer {
	char data[8];
};

/**
 * Returns the file name suffix, ignoring the query string.
 */
gcc_pure
const char *
uri_get_suffix(const char *uri, UriSuffixBuffer &buffer);

/**
 * Returns true if this is a safe "local" URI:
 *
 * - non-empty
 * - does not begin or end with a slash
 * - no double slashes
 * - no path component begins with a dot
 */
gcc_pure
bool
uri_safe_local(const char *uri);

/**
 * Removes HTTP username and password from the URI.  This may be
 * useful for displaying an URI without disclosing secrets.  Returns
 * an empty string if nothing needs to be removed, or if the URI is
 * not recognized.
 */
gcc_pure
std::string
uri_remove_auth(const char *uri);

/**
 * Check whether #child specifies a resource "inside" the directory
 * specified by #parent.  If the strings are equal, the function
 * returns false.
 */
gcc_pure gcc_nonnull_all
bool
uri_is_child(const char *parent, const char *child);

gcc_pure gcc_nonnull_all
bool
uri_is_child_or_same(const char *parent, const char *child);

/**
 * Translate the given URI in the context of #base.  For example,
 * uri_apply_base("foo", "http://bar/a/")=="http://bar/a/foo".
 */
gcc_pure
std::string
uri_apply_base(const std::string &uri, const std::string &base);

#endif
