/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Mapper.hxx"
#include "Idle.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "util/UriUtil.hxx"

#include <stdexcept>

void
playlist_print_song(BufferedOutputStream &os, const DetachedSong &song)
{
	const char *uri_utf8 = playlist_saveAbsolutePaths
		? song.GetRealURI()
		: song.GetURI();

	try {
		const auto uri_fs = AllocatedPath::FromUTF8Throw(uri_utf8);
		os.Format("%s\n", NarrowPath(uri_fs).c_str());
	} catch (const std::runtime_error &) {
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
			os.Format("%s\n", NarrowPath(path).c_str());
	} catch (const std::runtime_error &) {
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
