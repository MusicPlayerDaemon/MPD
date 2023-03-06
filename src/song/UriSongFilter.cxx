// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UriSongFilter.hxx"
#include "Escape.hxx"
#include "LightSong.hxx"

std::string
UriSongFilter::ToExpression() const noexcept
{
	return std::string("(file ") + filter.GetOperator()
		+ " \"" + EscapeFilterString(filter.GetValue()) + "\")";
}

bool
UriSongFilter::Match(const LightSong &song) const noexcept
{
	return filter.Match(song.GetURI().c_str());
}
