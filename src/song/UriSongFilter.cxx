// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UriSongFilter.hxx"
#include "Escape.hxx"
#include "LightSong.hxx"

#include <fmt/format.h>

using std::string_view_literals::operator""sv;

std::string
UriSongFilter::ToExpression() const noexcept
{
	return fmt::format("(file {} \"{}\")"sv,
			   filter.GetOperator(), EscapeFilterString(filter.GetValue()));
}

bool
UriSongFilter::Match(const LightSong &song) const noexcept
{
	return filter.Match(song.GetURI().c_str());
}
