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
#include "Mapper.hxx"
#include "DatabaseSong.hxx"
#include "ls.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "DetachedSong.hxx"

#include <assert.h>
#include <string.h>

static void
merge_song_metadata(DetachedSong &dest, const DetachedSong &base,
		    const DetachedSong &add)
{
	{
		TagBuilder builder(add.GetTag());
		builder.Complement(base.GetTag());
		dest.SetTag(builder.Commit());
	}

	dest.SetLastModified(base.GetLastModified());
	dest.SetStartMS(add.GetStartMS());
	dest.SetEndMS(add.GetEndMS());
}

static DetachedSong *
apply_song_metadata(DetachedSong *dest, const DetachedSong &src)
{
	assert(dest != nullptr);

	if (!src.GetTag().IsDefined() &&
	    src.GetStartMS() == 0 && src.GetEndMS() == 0)
		return dest;

	DetachedSong *tmp = new DetachedSong(dest->GetURI());
	merge_song_metadata(*tmp, *dest, src);

	if (dest->GetTag().IsDefined() && dest->GetTag().time > 0 &&
	    src.GetStartMS() > 0 && src.GetEndMS() == 0 &&
	    src.GetStartMS() / 1000 < (unsigned)dest->GetTag().time)
		/* the range is open-ended, and the playlist plugin
		   did not know the total length of the song file
		   (e.g. last track on a CUE file); fix it up here */
		tmp->WritableTag().time =
			dest->GetTag().time - src.GetStartMS() / 1000;

	delete dest;
	return tmp;
}

static DetachedSong *
playlist_check_load_song(const DetachedSong *song, const char *uri)
{
	DetachedSong *dest;

	if (uri_has_scheme(uri)) {
		dest = new DetachedSong(uri);
	} else if (PathTraitsUTF8::IsAbsolute(uri)) {
		dest = new DetachedSong(uri);
		if (!dest->Update()) {
			delete dest;
			return nullptr;
		}
	} else {
		dest = DatabaseDetachSong(uri, IgnoreError());
		if (dest == nullptr)
			return nullptr;
	}

	return apply_song_metadata(dest, *song);
}

DetachedSong *
playlist_check_translate_song(DetachedSong *song, const char *base_uri,
			      bool secure)
{
	const char *uri = song->GetURI();

	if (uri_has_scheme(uri)) {
		if (uri_supported_scheme(uri))
			/* valid remote song */
			return song;
		else {
			/* unsupported remote song */
			delete song;
			return nullptr;
		}
	}

	if (base_uri != nullptr && strcmp(base_uri, ".") == 0)
		/* PathTraitsUTF8::GetParent() returns "." when there
		   is no directory name in the given path; clear that
		   now, because it would break the database lookup
		   functions */
		base_uri = nullptr;

	if (PathTraitsUTF8::IsAbsolute(uri)) {
		/* XXX fs_charset vs utf8? */
		const char *suffix = map_to_relative_path(uri);
		assert(suffix != nullptr);

		if (suffix != uri)
			uri = suffix;
		else if (!secure) {
			/* local files must be relative to the music
			   directory when "secure" is enabled */
			delete song;
			return nullptr;
		}

		base_uri = nullptr;
	}

	if (base_uri != nullptr) {
		song->SetURI(PathTraitsUTF8::Build(base_uri, uri));
		/* repeat the above checks */
		return playlist_check_translate_song(song, nullptr, secure);
	}

	DetachedSong *dest = playlist_check_load_song(song, uri);
	delete song;

	return dest;
}
