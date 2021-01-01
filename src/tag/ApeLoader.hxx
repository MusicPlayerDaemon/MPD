/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_APE_LOADER_HXX
#define MPD_APE_LOADER_HXX

#include <functional>

struct StringView;
class InputStream;

typedef std::function<bool(unsigned long flags, const char *key,
			   StringView value)> ApeTagCallback;

/**
 * Scans the APE tag values from a file.
 *
 * Throws on I/O error.
 *
 * @return false if the file could not be opened or if no APE tag is
 * present
 */
bool
tag_ape_scan(InputStream &is, const ApeTagCallback& callback);

#endif
