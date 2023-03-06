// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Selection.hxx"
#include "Queue.hxx"
#include "song/DetachedSong.hxx"
#include "song/Filter.hxx"
#include "song/LightSong.hxx"

bool
QueueSelection::MatchPosition(const Queue &queue,
			      unsigned position) const noexcept
{
	if (filter != nullptr) {
		const auto song = queue.GetLight(position);
		if (!filter->Match(song))
			return false;
	}

	return true;
}
