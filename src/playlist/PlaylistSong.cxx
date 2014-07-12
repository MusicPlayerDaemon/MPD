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
#include "PlaylistSong.hxx"
#include "SongLoader.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "fs/Traits.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "DetachedSong.hxx"

#include <assert.h>
#include <string.h>

static void
merge_song_metadata(DetachedSong &add, const DetachedSong &base)
{
	if (base.GetTag().IsDefined()) {
		TagBuilder builder(add.GetTag());
		builder.Complement(base.GetTag());
		add.SetTag(builder.Commit());
	}

	add.SetLastModified(base.GetLastModified());
}

static bool
playlist_check_load_song(DetachedSong &song, const SongLoader &loader)
{
	DetachedSong *tmp = loader.LoadSong(song.GetURI(), IgnoreError());
	if (tmp == nullptr)
		return false;

	song.SetURI(tmp->GetURI());
	if (!song.HasRealURI() && tmp->HasRealURI())
		song.SetRealURI(tmp->GetRealURI());

	merge_song_metadata(song, *tmp);
	delete tmp;
	return true;
}

bool
playlist_check_translate_song(DetachedSong &song, const char *base_uri,
			      const SongLoader &loader)
{
	if (base_uri != nullptr && strcmp(base_uri, ".") == 0)
		/* PathTraitsUTF8::GetParent() returns "." when there
		   is no directory name in the given path; clear that
		   now, because it would break the database lookup
		   functions */
		base_uri = nullptr;

	const char *uri = song.GetURI();
	if (base_uri != nullptr && !uri_has_scheme(uri) &&
	    !PathTraitsUTF8::IsAbsolute(uri))
		song.SetURI(PathTraitsUTF8::Build(base_uri, uri));

	return playlist_check_load_song(song, loader);
}
