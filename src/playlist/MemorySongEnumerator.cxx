// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MemorySongEnumerator.hxx"

std::unique_ptr<DetachedSong>
MemorySongEnumerator::NextSong()
{
	if (songs.empty())
		return nullptr;

	auto result = std::make_unique<DetachedSong>(std::move(songs.front()));
	songs.pop_front();
	return result;
}
