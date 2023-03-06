// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "BaseSongFilter.hxx"
#include "Escape.hxx"
#include "LightSong.hxx"
#include "util/UriRelative.hxx"

std::string
BaseSongFilter::ToExpression() const noexcept
{
	return "(base \"" + EscapeFilterString(value) + "\")";
}

bool
BaseSongFilter::Match(const LightSong &song) const noexcept
{
	return uri_is_child_or_same(value.c_str(), song.GetURI().c_str());
}
