/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_MIME_TYPE_HXX
#define MPD_MIME_TYPE_HXX

#include <string>
#include <map>

/**
 * Extract the part of the MIME type before the parameters, i.e. the
 * part before the semicolon.  If there is no semicolon, it returns
 * the string as-is.
 */
std::string
GetMimeTypeBase(const char *s) noexcept;

/**
 * Parse the parameters from a MIME type string.  Parameters are
 * separated by semicolon.  Example:
 *
 * "foo/bar; param1=value1; param2=value2"
 */
std::map<std::string, std::string>
ParseMimeTypeParameters(const char *s) noexcept;

#endif
