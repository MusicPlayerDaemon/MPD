// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LocateUri.hxx"
#include "PlaylistQueue.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "PlaylistError.hxx"
#include "queue/Playlist.hxx"
#include "SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "input/Error.hxx"
#include "thread/Mutex.hxx"
#include "fs/Traits.hxx"
#include "Log.hxx"

#ifdef ENABLE_DATABASE
#include "SongLoader.hxx"
#endif

#include <memory>

void
playlist_load_into_queue(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
{
	const unsigned max_log_msgs = 8;

	const auto base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: ".";

	std::unique_ptr<DetachedSong> song;
	for (unsigned i = 0, failures = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			continue;
		}

		if (!playlist_check_translate_song(*song, base_uri,
						   loader)) {
			failures += 1;
			if (failures < max_log_msgs) {
				FmtError(playlist_domain, "Failed to load {:?}.", song->GetURI());
			} else if (failures == max_log_msgs) {
				LogError(playlist_domain, "Further errors for this playlist will not be logged.");
			}
			continue;
		}

		dest.AppendSong(pc, std::move(*song));
	}
}

void
playlist_open_into_queue(const LocatedUri &uri,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
try {
	Mutex mutex;

	auto playlist = playlist_open_any(uri,
#ifdef ENABLE_DATABASE
					  loader.GetStorage(),
#endif
					  mutex);
	if (playlist == nullptr)
		throw PlaylistError::NoSuchList();

	playlist_load_into_queue(uri.canonical_uri, *playlist,
				 start_index, end_index,
				 dest, pc, loader);
} catch (...) {
	if (IsFileNotFound(std::current_exception()))
		throw PlaylistError::NoSuchList();

	throw;
}
