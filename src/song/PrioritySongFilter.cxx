// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PrioritySongFilter.hxx"
#include "LightSong.hxx"
#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/format.h>

std::string
PrioritySongFilter::ToExpression() const noexcept
{
	return fmt::format(FMT_STRING("(prio >= {})"), value);
}

bool
PrioritySongFilter::Match(const LightSong &song) const noexcept
{
	return song.priority >= value;
}
