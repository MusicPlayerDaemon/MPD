// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistDirectory.hxx"
#include "PlaylistError.hxx"
#include "PlaylistFile.hxx" // for spl_valid_name()

#include <fmt/format.h>

using std::string_view_literals::operator""sv;

AllocatedPath
PlaylistDirectory::MapNameToPath(std::string_view name_utf8) const noexcept
{
	if (!spl_valid_name(name_utf8))
		return nullptr;

	const auto filename_utf8 = fmt::format("{}" PLAYLIST_FILE_SUFFIX ""sv, name_utf8);
	const auto filename_fs =
		AllocatedPath::FromUTF8(filename_utf8);
	if (filename_fs.IsNull())
		return nullptr;

	return path / filename_fs;
}

AllocatedPath
PlaylistDirectory::MapNameToPathOrThrow(std::string_view name_utf8) const
{
	auto result = MapNameToPath(name_utf8);
	if (result.IsNull())
		throw PlaylistError{PlaylistResult::BAD_NAME, "Bad playlist name"};

	return result;
}
