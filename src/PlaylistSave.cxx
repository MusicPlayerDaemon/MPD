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
#include "util/Alloc.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <string.h>

void
playlist_print_song(FILE *file, const DetachedSong &song)
{
	const char *uri_utf8 = playlist_saveAbsolutePaths
		? song.GetRealURI()
		: song.GetURI();

	const auto uri_fs = AllocatedPath::FromUTF8(uri_utf8);
	if (!uri_fs.IsNull())
		fprintf(file, "%s\n", uri_fs.c_str());
}

void
playlist_print_uri(FILE *file, const char *uri)
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
		fprintf(file, "%s\n", path.c_str());
}

PlaylistResult
spl_save_queue(const char *name_utf8, const Queue &queue)
{
	if (map_spl_path().IsNull())
		return PlaylistResult::DISABLED;

	if (!spl_valid_name(name_utf8))
		return PlaylistResult::BAD_NAME;

	const auto path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs.IsNull())
		return PlaylistResult::BAD_NAME;

	if (FileExists(path_fs))
		return PlaylistResult::LIST_EXISTS;

	FILE *file = FOpen(path_fs, FOpenMode::WriteText);

	if (file == nullptr)
		return PlaylistResult::ERRNO;

	for (unsigned i = 0; i < queue.GetLength(); i++)
		playlist_print_song(file, queue.Get(i));

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return PlaylistResult::SUCCESS;
}

PlaylistResult
spl_save_playlist(const char *name_utf8, const playlist &playlist)
{
	return spl_save_queue(name_utf8, playlist.queue);
}
