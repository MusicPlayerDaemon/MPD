// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "LocateUri.hxx"
#include "Length.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "SongEnumerator.hxx"
#include "SongPrint.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "input/Error.hxx"
#include "fs/Traits.hxx"
#include "thread/Mutex.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "PlaylistError.hxx"

#include <fmt/format.h>

static SignedSongTime get_duration(const DetachedSong &song) {
	const auto duration = song.GetDuration();
	return duration.IsNegative() ? (SignedSongTime)0 : song.GetDuration();
}

static void
playlist_provider_length(Response &r,
			const SongLoader &loader,
			const char *uri,
			SongEnumerator &e) noexcept
{
	const auto base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: ".";

	std::unique_ptr<DetachedSong> song;
	unsigned i = 0;
	SignedSongTime playtime = (SignedSongTime)0;
	while ((song = e.NextSong()) != nullptr) {
		if (playlist_check_translate_song(*song, base_uri,
						  loader))
			playtime += get_duration(*song);
		i++;
	}
	r.Fmt(FMT_STRING("songs: {}\n"), i);
	r.Fmt(FMT_STRING("playtime: {}\n"), playtime.RoundS());
}

void
playlist_file_length(Response &r, Partition &partition,
		     const SongLoader &loader,
		     const LocatedUri &uri)
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

	playlist_provider_length(r, loader, uri.canonical_uri, *playlist);
} catch (...) {
	if (IsFileNotFound(std::current_exception()))
		throw PlaylistError::NoSuchList();

	throw;
}
