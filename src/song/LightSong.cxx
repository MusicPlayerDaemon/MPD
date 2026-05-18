// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LightSong.hxx"
#include "tag/Tag.hxx"

std::string LightSong::GetURI() const noexcept
{
	if (directory == nullptr)
		return std::string(uri);

	std::string result(directory);
	result.push_back('/');
	result.append(uri);
	return result;
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
