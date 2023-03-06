// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ModifiedSinceSongFilter.hxx"
#include "LightSong.hxx"
#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"

std::string
ModifiedSinceSongFilter::ToExpression() const noexcept
{
	return std::string("(modified-since \"") + FormatISO8601(value).c_str() + "\")";
}

bool
ModifiedSinceSongFilter::Match(const LightSong &song) const noexcept
{
	return song.mtime >= value;
}
