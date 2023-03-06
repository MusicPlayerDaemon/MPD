// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

#include <fmt/format.h>

static void
playlist_print_path(BufferedOutputStream &os, const Path path)
{
#ifdef _UNICODE
	/* on Windows, playlists always contain UTF-8, because its
	   "narrow" charset (i.e. CP_ACP) is incapable of storing all
	   Unicode paths */
	try {
		os.Fmt(FMT_STRING("{}\n"), path.ToUTF8Throw());
	} catch (...) {
	}
#else
	os.Fmt(FMT_STRING("{}\n"), path.c_str());
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
spl_save_queue(const char *name_utf8, PlaylistSaveMode save_mode, const Queue &queue)
{
	const auto path_fs = spl_map_to_fs(name_utf8);
	assert(!path_fs.IsNull());

	if (save_mode == PlaylistSaveMode::CREATE) {
		if (FileExists(path_fs)) {
			throw PlaylistError(PlaylistResult::LIST_EXISTS, "Playlist already exists");
		}
	}
	else if (!FileExists(path_fs)) {
		throw PlaylistError(PlaylistResult::NO_SUCH_LIST, "No such playlist");
	}

	FileOutputStream fos(path_fs,
			     save_mode == PlaylistSaveMode::APPEND
			     ? FileOutputStream::Mode::APPEND_EXISTING
			     : FileOutputStream::Mode::CREATE);

	BufferedOutputStream bos(fos);

	for (unsigned i = 0; i < queue.GetLength(); i++)
		playlist_print_song(bos, queue.Get(i));

	bos.Flush();
	fos.Commit();

	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_save_playlist(const char *name_utf8, PlaylistSaveMode save_mode, const playlist &playlist)
{
	spl_save_queue(name_utf8, save_mode, playlist.queue);
}
