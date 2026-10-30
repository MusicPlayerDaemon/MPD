// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "fs/AllocatedPath.hxx"

#define PLAYLIST_FILE_SUFFIX ".m3u"

class PlaylistDirectory {
	const AllocatedPath path;

public:
	explicit PlaylistDirectory(AllocatedPath &&_path) noexcept
		:path(std::move(_path)) {}

	const AllocatedPath &GetPath() const noexcept {
		return path;
	}

	/**
	 * Maps a playlist name (without the ".m3u" suffix) to a file
	 * system path.
	 *
	 * @return the path in file system encoding, or nullptr if
	 * mapping failed.
	 */
	[[gnu::pure]]
	AllocatedPath MapNameToPath(std::string_view name_utf8) const noexcept;

	/**
	 * Same as MapNameToPath(), but throw #PlaylistError if the
	 * path conversion fails.
	 */
	AllocatedPath MapNameToPathOrThrow(std::string_view name_utf8) const;
};
