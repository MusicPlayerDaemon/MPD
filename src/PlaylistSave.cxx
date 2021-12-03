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

#include "config.h"
#include "PlaylistSave.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistError.hxx"
#include "queue/Playlist.hxx"
#include "song/DetachedSong.hxx"
#include "Mapper.hxx"
#include "Idle.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "util/UriExtract.hxx"

static void
playlist_print_path(BufferedOutputStream &os, const Path path)
{
#ifdef _UNICODE
	/* on Windows, playlists always contain UTF-8, because its
	   "narrow" charset (i.e. CP_ACP) is incapable of storing all
	   Unicode paths */
	try {
		os.Format("%s\n", path.ToUTF8Throw().c_str());
	} catch (...) {
	}
#else
	os.Format("%s\n", path.c_str());
#endif
}

void
playlist_print_song(BufferedOutputStream &os, const DetachedSong &song)
{
	const char *uri_utf8 = playlist_saveAbsolutePaths
		? song.GetRealURI()
		: song.GetURI();

	try {
		const auto uri_fs = AllocatedPath::FromUTF8Throw(uri_utf8);
		playlist_print_path(os, uri_fs);
	} catch (...) {
	}
}

void
playlist_print_uri(BufferedOutputStream &os, const char *uri)
{
	try {
		auto path =
#ifdef ENABLE_DATABASE
			playlist_saveAbsolutePaths && !uri_has_scheme(uri) &&
			!PathTraitsUTF8::IsAbsolute(uri)
			? map_uri_fs(uri)
			:
#endif
			AllocatedPath::FromUTF8Throw(uri);

		if (!path.IsNull())
			playlist_print_path(os, path);
	} catch (...) {
	}
}

void
spl_save_queue(const char *name_utf8, const Queue &queue)
{
	const auto path_fs = spl_map_to_fs(name_utf8);
	assert(!path_fs.IsNull());

	if (FileExists(path_fs))
		throw PlaylistError(PlaylistResult::LIST_EXISTS,
				    "Playlist already exists");

	FileOutputStream fos(path_fs);
	BufferedOutputStream bos(fos);

	for (unsigned i = 0; i < queue.GetLength(); i++)
		playlist_print_song(bos, queue.Get(i));

	bos.Flush();
	fos.Commit();

	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_save_playlist(const char *name_utf8, const playlist &playlist)
{
	spl_save_queue(name_utf8, playlist.queue);
}
