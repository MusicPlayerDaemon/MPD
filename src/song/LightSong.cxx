// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LightSong.hxx"
#include "tag/Tag.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

std::string LightSong::GetURI() const noexcept
{
	if (directory == nullptr)
		return std::string(uri);

	return fmt::format("{}/{}"sv, directory, uri);
}

SignedSongTime
LightSong::GetDuration() const noexcept
{
	SongTime a = start_time, b = end_time;
	if (!b.IsPositive()) {
		if (tag.duration.IsNegative())
			return tag.duration;

		b = SongTime(tag.duration);
	}

	return {b - a};
}
