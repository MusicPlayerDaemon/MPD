/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "PlaylistAny.hxx"
#include "PlaylistStream.hxx"
#include "PlaylistMapper.hxx"
#include "SongEnumerator.hxx"
#include "util/UriUtil.hxx"

std::unique_ptr<SongEnumerator>
playlist_open_any(const char *uri,
#ifdef ENABLE_DATABASE
		  const Storage *storage,
#endif
		  Mutex &mutex)
{
	return uri_has_scheme(uri)
		? playlist_open_remote(uri, mutex)
		: playlist_mapper_open(uri,
#ifdef ENABLE_DATABASE
				       storage,
#endif
				       mutex);
}
