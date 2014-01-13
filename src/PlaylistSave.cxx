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
#include "Playlist.hxx"
#include "Song.hxx"
#include "DetachedSong.hxx"
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
	if (playlist_saveAbsolutePaths && song.IsInDatabase()) {
		const auto path = map_song_fs(song);
		if (!path.IsNull())
			fprintf(file, "%s\n", path.c_str());
	} else {
		const auto uri_utf8 = song.GetURI();
		const auto uri_fs = AllocatedPath::FromUTF8(uri_utf8);

		if (!uri_fs.IsNull())
			fprintf(file, "%s\n", uri_fs.c_str());
	}
}

void
playlist_print_uri(FILE *file, const char *uri)
{
	auto path = playlist_saveAbsolutePaths && !uri_has_scheme(uri) &&
		!PathTraitsUTF8::IsAbsolute(uri)
		? map_uri_fs(uri)
		: AllocatedPath::FromUTF8(uri);

	if (!path.IsNull())
		fprintf(file, "%s\n", path.c_str());
}

PlaylistResult
spl_save_queue(const char *name_utf8, const queue &queue)
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

bool
playlist_load_spl(struct playlist &playlist, PlayerControl &pc,
		  const char *name_utf8,
		  unsigned start_index, unsigned end_index,
		  Error &error)
{
	PlaylistFileContents contents = LoadPlaylistFile(name_utf8, error);
	if (contents.empty() && error.IsDefined())
		return false;

	if (end_index > contents.size())
		end_index = contents.size();

	for (unsigned i = start_index; i < end_index; ++i) {
		const auto &uri_utf8 = contents[i];

		if (memcmp(uri_utf8.c_str(), "file:///", 8) == 0) {
			const char *path_utf8 = uri_utf8.c_str() + 7;

			if (playlist.AppendFile(pc, path_utf8) != PlaylistResult::SUCCESS)
				FormatError(playlist_domain,
					    "can't add file \"%s\"", path_utf8);
			continue;
		}

		if ((playlist.AppendURI(pc, uri_utf8.c_str())) != PlaylistResult::SUCCESS) {
			/* for windows compatibility, convert slashes */
			char *temp2 = xstrdup(uri_utf8.c_str());
			char *p = temp2;
			while (*p) {
				if (*p == '\\')
					*p = '/';
				p++;
			}

			if (playlist.AppendURI(pc, temp2) != PlaylistResult::SUCCESS)
				FormatError(playlist_domain,
					    "can't add file \"%s\"", temp2);

			free(temp2);
		}
	}

	return true;
}
