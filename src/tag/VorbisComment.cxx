// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VorbisComment.hxx"
#include "util/StringCompare.hxx"

#include <cassert>

std::string_view
GetVorbisCommentValue(std::string_view entry, std::string_view name) noexcept
{
	assert(!name.empty());

	if (StringStartsWithIgnoreCase(entry, name) &&
	    entry.size() > name.size() &&
	    entry[name.size()] == '=')
		return entry.substr(name.size() + 1);

	return {};
}
