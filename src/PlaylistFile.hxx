// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "fs/AllocatedPath.hxx"

#include <vector>
#include <string>
#include <string_view>

struct ConfigData;
struct RangeArg;
class DetachedSong;
class SongLoader;
class PlaylistVector;

typedef std::vector<std::string> PlaylistFileContents;

extern bool playlist_saveAbsolutePaths;

class PlaylistFileEditor {
	const AllocatedPath path;

	PlaylistFileContents contents;

public:
	enum class LoadMode {
		NO,
		YES,
		TRY,
	};

	/**
	 * Throws on error.
	 */
	explicit PlaylistFileEditor(const char *name_utf8, LoadMode load_mode);

	auto size() const noexcept {
		return contents.size();
	}

	void Insert(std::size_t i, std::string_view uri);
	void Insert(std::size_t i, const DetachedSong &song);

	void MoveIndex(RangeArg src, unsigned dest);
	void RemoveIndex(unsigned i);
	void RemoveRange(RangeArg range);

	void Save();

private:
	void Load();
};

/**
 * Perform some global initialization, e.g. load configuration values.
 */
void
spl_global_init(const ConfigData &config);

/**
 * Determines whether the specified string is a valid name for a
 * stored playlist.
 */
[[gnu::pure]]
bool
spl_valid_name(std::string_view name_utf8) noexcept;

[[nodiscard]]
AllocatedPath
spl_map_to_fs(std::string_view name_utf8);

/**
 * Returns a list of stored_playlist_info struct pointers.
 */
[[nodiscard]]
PlaylistVector
ListPlaylistFiles();

void
spl_clear(std::string_view utf8path);

void
spl_delete(std::string_view name_utf8);

void
spl_append_song(std::string_view utf8path, const DetachedSong &song);

/**
 * Throws #std::runtime_error on error.
 */
void
spl_append_uri(std::string_view path_utf8,
	       const SongLoader &loader, const char *uri_utf8);

void
spl_rename(std::string_view utf8from, std::string_view utf8to);
