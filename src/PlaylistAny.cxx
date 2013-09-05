/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "InputStream.hxx"

#include <assert.h>

static SongEnumerator *
playlist_open_remote(const char *uri, Mutex &mutex, Cond &cond,
		     struct input_stream **is_r)
{
	assert(uri_has_scheme(uri));

	SongEnumerator *playlist = playlist_list_open_uri(uri, mutex, cond);
	if (playlist != NULL) {
		*is_r = NULL;
		return playlist;
	}

	Error error;
	input_stream *is = input_stream::Open(uri, mutex, cond, error);
	if (is == NULL) {
		if (error.IsDefined())
			g_warning("Failed to open %s: %s",
				  uri, error.GetMessage());

		return NULL;
	}

	playlist = playlist_list_open_stream(is, uri);
	if (playlist == NULL) {
		is->Close();
		return NULL;
	}

	*is_r = is;
	return playlist;
}

SongEnumerator *
playlist_open_any(const char *uri, Mutex &mutex, Cond &cond,
		  struct input_stream **is_r)
{
	return uri_has_scheme(uri)
		? playlist_open_remote(uri, mutex, cond, is_r)
		: playlist_mapper_open(uri, mutex, cond, is_r);
}
