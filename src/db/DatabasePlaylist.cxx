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
#include "DatabasePlaylist.hxx"
#include "DatabaseSong.hxx"
#include "Selection.hxx"
#include "PlaylistFile.hxx"
#include "Interface.hxx"
#include "DetachedSong.hxx"
#include "storage/StorageInterface.hxx"
#include "db/Selection.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/FileOutputStream.hxx"
#include "playlist/SongEnumerator.hxx"
#include "playlist/PlaylistSong.hxx"
#include "playlist/PlaylistAny.hxx"
#include "PlaylistSave.hxx"
#include "PlaylistFile.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/StringUtil.hxx"
#include "util/StringCompare.hxx"
#include "PlaylistError.hxx"
#include "fs/FileSystem.hxx"

#include <functional>

#ifdef ENABLE_DATABASE
#include "SongLoader.hxx"
#endif

static void
AddSong(const Storage *storage, const char *playlist_path_utf8,
	const LightSong &song)
{
	spl_append_song(playlist_path_utf8,
			DatabaseDetachSong(storage, song));
}

void
search_add_to_playlist(const Database &db, const Storage *storage,
		       const char *playlist_path_utf8,
		       const DatabaseSelection &selection)
{
	using namespace std::placeholders;
	const auto f = std::bind(AddSong, std::ref(storage),
				 playlist_path_utf8, _1);
	db.Visit(selection, f);
}

// throw PlaylistError::NoSuchList()
static void
playlist_load_into_playlist(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 const char *dest,
			 const SongLoader &loader,
			 BufferedOutputStream *bos)
{
	const std::string base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: std::string(".");

	std::unique_ptr<DetachedSong> song;
	for (unsigned i = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			continue;
		}

		if (!playlist_check_translate_song(*song, base_uri.c_str(),
						   loader)) {
			continue;
		}

		if (bos != nullptr) {
			playlist_print_song(*bos, *song, true);
		}
		spl_append_song(dest,std::move(*song));
	}
}

// throw PlaylistError::NoSuchList()
void
playlist_open_into_playlist(const char *uri,
			 unsigned start_index, unsigned end_index,
			 const char *dest,
			 const SongLoader &loader)
{
	Mutex mutex;
	Cond cond;
	FileOutputStream *fos = nullptr;
	BufferedOutputStream *bos = nullptr;
	std::string new_uri = uri;

	bool full = StringStartsWith(uri, "upnp_");
	if (full) {
		if (!is_mpd_playlist_file(uri)) {
			new_uri.insert(0, "temp_");
			const auto path_fs = spl_map_to_fs(new_uri.c_str());
			fos = new FileOutputStream(path_fs);
			bos = new BufferedOutputStream(*fos);
			bos->Write("#MPDM3U\n");
		}
	}

	auto playlist = playlist_open_any(uri,
#ifdef ENABLE_DATABASE
					  loader.GetStorage(),
#endif
					  mutex, cond);
	if (playlist == nullptr) {
		throw PlaylistError::NoSuchList();
	}

	playlist_load_into_playlist(uri, *playlist,
					 start_index, end_index,
					 dest, loader, bos);
	if (bos!=nullptr && fos!= nullptr) {
		bos->Flush();
		fos->Commit();
		const auto path_fs = spl_map_to_fs(uri);
		const auto new_path_fs = spl_map_to_fs(new_uri.c_str());
		RemoveFile(path_fs);
		RenameFile(new_path_fs, path_fs);
	}
}
