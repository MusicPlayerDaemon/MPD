/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Maps directory and song objects to file system paths.
 */

#include "config.h"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/Charset.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <errno.h>

static constexpr Domain mapper_domain("mapper");

#ifdef ENABLE_DATABASE

/**
 * The absolute path of the music directory encoded in the filesystem
 * character set.
 */
static AllocatedPath music_dir_fs = AllocatedPath::Null();

#endif

/**
 * The absolute path of the playlist directory encoded in the
 * filesystem character set.
 */
static AllocatedPath playlist_dir_fs = AllocatedPath::Null();

static void
check_directory(const char *path_utf8, const AllocatedPath &path_fs)
{
	struct stat st;
	if (!StatFile(path_fs, st)) {
		FormatErrno(mapper_domain,
			    "Failed to stat directory \"%s\"",
			    path_utf8);
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		FormatError(mapper_domain,
			    "Not a directory: %s", path_utf8);
		return;
	}

#ifndef WIN32
	const auto x = AllocatedPath::Build(path_fs, ".");
	if (!StatFile(x, st) && errno == EACCES)
		FormatError(mapper_domain,
			    "No permission to traverse (\"execute\") directory: %s",
			    path_utf8);
#endif

	const DirectoryReader reader(path_fs);
	if (reader.HasFailed() && errno == EACCES)
		FormatError(mapper_domain,
			    "No permission to read directory: %s", path_utf8);
}

#ifdef ENABLE_DATABASE

static void
mapper_set_music_dir(AllocatedPath &&path)
{
	assert(!path.IsNull());

	music_dir_fs = std::move(path);
	music_dir_fs.ChopSeparators();

	const auto music_dir_utf8 = music_dir_fs.ToUTF8();

	check_directory(music_dir_utf8.c_str(), music_dir_fs);
}

#endif

static void
mapper_set_playlist_dir(AllocatedPath &&path)
{
	assert(!path.IsNull());

	playlist_dir_fs = std::move(path);

	const auto utf8 = playlist_dir_fs.ToUTF8();
	check_directory(utf8.c_str(), playlist_dir_fs);
}

void
mapper_init(AllocatedPath &&_music_dir, AllocatedPath &&_playlist_dir)
{
#ifdef ENABLE_DATABASE
	if (!_music_dir.IsNull())
		mapper_set_music_dir(std::move(_music_dir));
#else
	(void)_music_dir;
#endif

	if (!_playlist_dir.IsNull())
		mapper_set_playlist_dir(std::move(_playlist_dir));
}

void mapper_finish(void)
{
}

#ifdef ENABLE_DATABASE

AllocatedPath
map_uri_fs(const char *uri)
{
	assert(uri != nullptr);
	assert(*uri != '/');

	if (music_dir_fs.IsNull())
		return AllocatedPath::Null();

	const auto uri_fs = AllocatedPath::FromUTF8(uri);
	if (uri_fs.IsNull())
		return AllocatedPath::Null();

	return AllocatedPath::Build(music_dir_fs, uri_fs);
}

std::string
map_fs_to_utf8(const char *path_fs)
{
	if (PathTraitsFS::IsSeparator(path_fs[0])) {
		path_fs = music_dir_fs.RelativeFS(path_fs);
		if (path_fs == nullptr || *path_fs == 0)
			return std::string();
	}

	return PathToUTF8(path_fs);
}

#endif

const AllocatedPath &
map_spl_path(void)
{
	return playlist_dir_fs;
}

AllocatedPath
map_spl_utf8_to_fs(const char *name)
{
	if (playlist_dir_fs.IsNull())
		return AllocatedPath::Null();

	std::string filename_utf8 = name;
	filename_utf8.append(PLAYLIST_FILE_SUFFIX);

	const auto filename_fs =
		AllocatedPath::FromUTF8(filename_utf8.c_str());
	if (filename_fs.IsNull())
		return AllocatedPath::Null();

	return AllocatedPath::Build(playlist_dir_fs, filename_fs);
}
