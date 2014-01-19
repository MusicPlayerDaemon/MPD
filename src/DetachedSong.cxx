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

#include "config.h"
#include "DetachedSong.hxx"
#include "LightSong.hxx"
#include "util/UriUtil.hxx"
#include "fs/Traits.hxx"

DetachedSong::DetachedSong(const LightSong &other)
	:uri(other.GetURI().c_str()),
	 tag(*other.tag),
	 mtime(other.mtime),
	 start_ms(other.start_ms), end_ms(other.end_ms) {}

bool
DetachedSong::IsRemote() const
{
	return uri_has_scheme(uri.c_str());
}

bool
DetachedSong::IsAbsoluteFile() const
{
	return PathTraitsUTF8::IsAbsolute(uri.c_str());
}

double
DetachedSong::GetDuration() const
{
	if (end_ms > 0)
		return (end_ms - start_ms) / 1000.0;

	return tag.time - start_ms / 1000.0;
}
