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
#include "PlaylistSong.hxx"
#include "Mapper.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseGlue.hxx"
#include "ls.hxx"
#include "tag.h"
#include "fs/Path.hxx"
#include "util/UriUtil.hxx"
#include "Song.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

static void
merge_song_metadata(Song *dest, const Song *base,
		    const Song *add)
{
	dest->tag = base->tag != NULL
		? (add->tag != NULL
		   ? tag_merge(base->tag, add->tag)
		   : tag_dup(base->tag))
		: (add->tag != NULL
		   ? tag_dup(add->tag)
		   : NULL);

	dest->mtime = base->mtime;
	dest->start_ms = add->start_ms;
	dest->end_ms = add->end_ms;
}

static Song *
apply_song_metadata(Song *dest, const Song *src)
{
	Song *tmp;

	assert(dest != NULL);
	assert(src != NULL);

	if (src->tag == NULL && src->start_ms == 0 && src->end_ms == 0)
		return dest;

	if (dest->IsInDatabase()) {
		const Path &path_fs = map_song_fs(dest);
		if (path_fs.IsNull())
			return dest;

		std::string path_utf8 = path_fs.ToUTF8();
		if (path_utf8.empty())
			path_utf8 = path_fs.c_str();

		tmp = Song::NewFile(path_utf8.c_str(), NULL);

		merge_song_metadata(tmp, dest, src);
	} else {
		tmp = Song::NewFile(dest->uri, NULL);
		merge_song_metadata(tmp, dest, src);
	}

	if (dest->tag != NULL && dest->tag->time > 0 &&
	    src->start_ms > 0 && src->end_ms == 0 &&
	    src->start_ms / 1000 < (unsigned)dest->tag->time)
		/* the range is open-ended, and the playlist plugin
		   did not know the total length of the song file
		   (e.g. last track on a CUE file); fix it up here */
		tmp->tag->time = dest->tag->time - src->start_ms / 1000;

	dest->Free();
	return tmp;
}

static Song *
playlist_check_load_song(const Song *song, const char *uri, bool secure)
{
	Song *dest;

	if (uri_has_scheme(uri)) {
		dest = Song::NewRemote(uri);
	} else if (g_path_is_absolute(uri) && secure) {
		dest = Song::LoadFile(uri, nullptr);
		if (dest == NULL)
			return NULL;
	} else {
		const Database *db = GetDatabase(nullptr);
		if (db == nullptr)
			return nullptr;

		Song *tmp = db->GetSong(uri, nullptr);
		if (tmp == NULL)
			/* not found in database */
			return NULL;

		dest = tmp->DupDetached();
		db->ReturnSong(tmp);
	}

	return apply_song_metadata(dest, song);
}

Song *
playlist_check_translate_song(Song *song, const char *base_uri,
			      bool secure)
{
	if (song->IsInDatabase())
		/* already ok */
		return song;

	const char *uri = song->uri;

	if (uri_has_scheme(uri)) {
		if (uri_supported_scheme(uri))
			/* valid remote song */
			return song;
		else {
			/* unsupported remote song */
			song->Free();
			return NULL;
		}
	}

	if (base_uri != NULL && strcmp(base_uri, ".") == 0)
		/* g_path_get_dirname() returns "." when there is no
		   directory name in the given path; clear that now,
		   because it would break the database lookup
		   functions */
		base_uri = NULL;

	if (g_path_is_absolute(uri)) {
		/* XXX fs_charset vs utf8? */
		const char *suffix = map_to_relative_path(uri);
		assert(suffix != NULL);

		if (suffix != uri)
			uri = suffix;
		else if (!secure) {
			/* local files must be relative to the music
			   directory when "secure" is enabled */
			song->Free();
			return NULL;
		}

		base_uri = NULL;
	}

	char *allocated = NULL;
	if (base_uri != NULL)
		uri = allocated = g_build_filename(base_uri, uri, NULL);

	Song *dest = playlist_check_load_song(song, uri, secure);
	song->Free();
	g_free(allocated);
	return dest;
}
