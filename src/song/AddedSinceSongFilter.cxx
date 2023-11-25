// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AddedSinceSongFilter.hxx"
#include "LightSong.hxx"
#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/core.h>

using std::string_view_literals::operator""sv;

std::string
AddedSinceSongFilter::ToExpression() const noexcept
{
	return fmt::format("(added-since \"{}\")"sv, FormatISO8601(value).c_str());
}

bool
AddedSinceSongFilter::Match(const LightSong &song) const noexcept
{
	return song.added >= value;
}
