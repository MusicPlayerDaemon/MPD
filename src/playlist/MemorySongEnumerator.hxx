// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_MEMORY_PLAYLIST_PROVIDER_HXX
#define MPD_MEMORY_PLAYLIST_PROVIDER_HXX

#include "SongEnumerator.hxx"
#include "song/DetachedSong.hxx"

#include <forward_list>

class MemorySongEnumerator final : public SongEnumerator {
	std::forward_list<DetachedSong> songs;

public:
	MemorySongEnumerator(std::forward_list<DetachedSong> &&_songs)
		:songs(std::move(_songs)) {}

	std::unique_ptr<DetachedSong> NextSong() override;
};

#endif
