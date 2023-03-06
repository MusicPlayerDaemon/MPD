// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_FILE_HXX
#define MPD_PLAYLIST_FILE_HXX

#include "fs/AllocatedPath.hxx"

#include <vector>
#include <string>

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

	void Insert(std::size_t i, const char *uri);
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
bool
spl_valid_name(const char *name_utf8);

AllocatedPath
spl_map_to_fs(const char *name_utf8);

/**
 * Returns a list of stored_playlist_info struct pointers.
 */
PlaylistVector
ListPlaylistFiles();

void
spl_clear(const char *utf8path);

void
spl_delete(const char *name_utf8);

void
spl_append_song(const char *utf8path, const DetachedSong &song);

/**
 * Throws #std::runtime_error on error.
 */
void
spl_append_uri(const char *path_utf8,
	       const SongLoader &loader, const char *uri_utf8);

void
spl_rename(const char *utf8from, const char *utf8to);

#endif
