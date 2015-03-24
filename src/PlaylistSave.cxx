/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "DetachedSong.hxx"
#include "SongLoader.hxx"
#include "Mapper.hxx"
#include "Idle.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "util/Alloc.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <string.h>

void
playlist_print_song(BufferedOutputStream &os, const DetachedSong &song)
{
	const char *uri_utf8 = playlist_saveAbsolutePaths
		? song.GetRealURI()
		: song.GetURI();

	const auto uri_fs = AllocatedPath::FromUTF8(uri_utf8);
	if (!uri_fs.IsNull())
		os.Format("%s\n", NarrowPath(uri_fs).c_str());
}

void
playlist_print_uri(BufferedOutputStream &os, const char *uri)
{
	auto path =
#ifdef ENABLE_DATABASE
		playlist_saveAbsolutePaths && !uri_has_scheme(uri) &&
		!PathTraitsUTF8::IsAbsolute(uri)
		? map_uri_fs(uri)
		:
#endif
		AllocatedPath::FromUTF8(uri);

	if (!path.IsNull())
		os.Format("%s\n", NarrowPath(path).c_str());
}

bool
spl_save_queue(const char *name_utf8, const Queue &queue, Error &error)
{
	const auto path_fs = spl_map_to_fs(name_utf8, error);
	if (path_fs.IsNull())
		return false;

	if (FileExists(path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::LIST_EXISTS),
			  "Playlist already exists");
		return false;
	}

	FileOutputStream fos(path_fs, error);
	if (!fos.IsDefined()) {
		TranslatePlaylistError(error);
		return false;
	}

	BufferedOutputStream bos(fos);

	for (unsigned i = 0; i < queue.GetLength(); i++)
		playlist_print_song(bos, queue.Get(i));

	if (!bos.Flush(error) || !fos.Commit(error))
		return false;

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_save_playlist(const char *name_utf8, const playlist &playlist,
		  Error &error)
{
	return spl_save_queue(name_utf8, playlist.queue, error);
}
