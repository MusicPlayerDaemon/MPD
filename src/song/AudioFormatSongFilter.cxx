// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AudioFormatSongFilter.hxx"
#include "LightSong.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

std::string
AudioFormatSongFilter::ToExpression() const noexcept
{
	return fmt::format("(AudioFormat {} \"{}\")"sv,
			   value.IsFullyDefined() ? "==" : "=~",
			   ToString(value).c_str());
}

bool
AudioFormatSongFilter::Match(const LightSong &song) const noexcept
{
	return song.audio_format.IsDefined() &&
		song.audio_format.MatchMask(value);
}
