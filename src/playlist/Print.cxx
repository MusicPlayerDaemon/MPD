// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LocateUri.hxx"
#include "Print.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "SongEnumerator.hxx"
#include "SongPrint.hxx"
#include "song/DetachedSong.hxx"
#include "input/Error.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "PlaylistError.hxx"

static void
playlist_provider_print(Response &r,
			const SongLoader &loader,
			const char *uri,
			SongEnumerator &e,
			unsigned start_index,
			unsigned end_index,
			bool detail) noexcept
{
	const auto base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: ".";

	std::unique_ptr<DetachedSong> song;

	for (unsigned i = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			continue;
		}

		if (playlist_check_translate_song(*song, base_uri,
						  loader) &&
		    detail)
			song_print_info(r, *song);
		else
			/* fallback if no detail was requested or no
			   detail was available */
			song_print_uri(r, *song);
	}
}

void
playlist_file_print(Response &r, Partition &partition,
		    const SongLoader &loader,
		    const LocatedUri &uri,
		    unsigned start_index,
		    unsigned end_index,
		    bool detail)
try {
	Mutex mutex;

#ifndef ENABLE_DATABASE
	(void)partition;
#endif

	auto playlist = playlist_open_any(uri,
#ifdef ENABLE_DATABASE
					  partition.instance.storage,
#endif
					  mutex);
	if (playlist == nullptr)
		throw PlaylistError::NoSuchList();

	playlist_provider_print(r, loader, uri.canonical_uri, *playlist,
				start_index, end_index, detail);
} catch (...) {
	if (IsFileNotFound(std::current_exception()))
		throw PlaylistError::NoSuchList();

	throw;
}
