// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LocateUri.hxx"
#include "Print.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "SongEnumerator.hxx"
#include "SongPrint.hxx"
#include "song/Filter.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
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

static void
playlist_provider_search_print(Response &r,
			       const SongLoader &loader,
			       const char *uri,
			       SongEnumerator &e,
			       unsigned start_index,
			       unsigned end_index,
			       SongFilter *filter) noexcept
{
	const auto base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: ".";

	std::unique_ptr<DetachedSong> song;

	unsigned skip = start_index;
	unsigned n = end_index - start_index;

	while ((song = e.NextSong()) != nullptr) {
		const bool detail = playlist_check_translate_song(*song, base_uri,
								  loader);
		if (!filter->Match(static_cast<LightSong>(*song)))
			continue;

		if (skip > 0) {
			--skip;
			continue;
		}

		if (detail)
			song_print_info(r, *song);
		else
			/* fallback if no detail was requested or no
			   detail was available */
			song_print_uri(r, *song);

		if (--n == 0)
			break;
	}
}

void
playlist_file_print(Response &r, Partition &partition,
		    const SongLoader &loader,
		    const LocatedUri &uri,
		    unsigned start_index,
		    unsigned end_index,
		    bool detail,
		    SongFilter *filter)
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

	if (filter == nullptr)
		playlist_provider_print(r, loader, uri.canonical_uri, *playlist,
					start_index, end_index, detail);
	else
		playlist_provider_search_print(r, loader, uri.canonical_uri, *playlist,
					start_index, end_index, filter);
} catch (...) {
	if (IsFileNotFound(std::current_exception()))
		throw PlaylistError::NoSuchList();

	throw;
}
