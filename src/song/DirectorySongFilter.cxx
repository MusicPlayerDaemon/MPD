// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DirectorySongFilter.hxx"
#include "Escape.hxx"
#include "LightSong.hxx"
#include "util/StringAPI.hxx"

std::string
DirectorySongFilter::ToExpression() const noexcept
{
	return "(directory \"" + EscapeFilterString(value) + "\")";
}

bool
DirectorySongFilter::Match(const LightSong &song) const noexcept
{
  return StringIsEqual(value.c_str(), song.directory == nullptr ? "" : song.directory);
}
