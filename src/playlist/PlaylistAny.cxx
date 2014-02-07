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
#include "PlaylistAny.hxx"
#include "PlaylistMapper.hxx"
#include "PlaylistRegistry.hxx"
#include "CloseSongEnumerator.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "input/InputStream.hxx"
#include "Log.hxx"

#include <assert.h>

static SongEnumerator *
playlist_open_remote(const char *uri, Mutex &mutex, Cond &cond)
{
	assert(uri_has_scheme(uri));

	SongEnumerator *playlist = playlist_list_open_uri(uri, mutex, cond);
	if (playlist != nullptr)
		return playlist;

	Error error;
	InputStream *is = InputStream::OpenReady(uri, mutex, cond, error);
	if (is == nullptr) {
		if (error.IsDefined())
			FormatError(error, "Failed to open %s", uri);

		return nullptr;
	}

	playlist = playlist_list_open_stream(*is, uri);
	if (playlist == nullptr) {
		is->Close();
		return nullptr;
	}

	return new CloseSongEnumerator(playlist, is);
}

SongEnumerator *
playlist_open_any(const char *uri, Mutex &mutex, Cond &cond)
{
	return uri_has_scheme(uri)
		? playlist_open_remote(uri, mutex, cond)
		: playlist_mapper_open(uri, mutex, cond);
}
