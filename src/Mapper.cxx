/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Directory.hxx"
#include "Song.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

static constexpr Domain mapper_domain("mapper");

/**
 * The absolute path of the music directory encoded in UTF-8.
 */
static char *music_dir_utf8;
static size_t music_dir_utf8_length;

/**
 * The absolute path of the music directory encoded in the filesystem
 * character set.
 */
static Path music_dir_fs = Path::Null();
static size_t music_dir_fs_length;

/**
 * The absolute path of the playlist directory encoded in the
 * filesystem character set.
 */
static Path playlist_dir_fs = Path::Null();

/**
 * Duplicate a string, chop all trailing slashes.
 */
static char *
strdup_chop_slash(const char *path_fs)
{
	size_t length = strlen(path_fs);

	while (length > 0 && path_fs[length - 1] == G_DIR_SEPARATOR)
		--length;

	return g_strndup(path_fs, length);
}

static void
check_directory(const char *path_utf8, const Path &path_fs)
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
	const Path x = Path::Build(path_fs, ".");
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

static void
mapper_set_music_dir(Path &&path)
{
	assert(!path.IsNull());

	music_dir_fs = std::move(path);
	music_dir_fs_length = music_dir_fs.length();

	const auto utf8 = music_dir_fs.ToUTF8();
	music_dir_utf8 = strdup_chop_slash(utf8.c_str());
	music_dir_utf8_length = strlen(music_dir_utf8);

	check_directory(music_dir_utf8, music_dir_fs);
}

static void
mapper_set_playlist_dir(Path &&path)
{
	assert(!path.IsNull());

	playlist_dir_fs = std::move(path);

	const auto utf8 = playlist_dir_fs.ToUTF8();
	check_directory(utf8.c_str(), playlist_dir_fs);
}

void
mapper_init(Path &&_music_dir, Path &&_playlist_dir)
{
	if (!_music_dir.IsNull())
		mapper_set_music_dir(std::move(_music_dir));

	if (!_playlist_dir.IsNull())
		mapper_set_playlist_dir(std::move(_playlist_dir));
}

void mapper_finish(void)
{
	g_free(music_dir_utf8);
}

const char *
mapper_get_music_directory_utf8(void)
{
	return music_dir_utf8;
}

const Path &
mapper_get_music_directory_fs(void)
{
	return music_dir_fs;
}

const char *
map_to_relative_path(const char *path_utf8)
{
	return music_dir_utf8 != NULL &&
		memcmp(path_utf8, music_dir_utf8,
		       music_dir_utf8_length) == 0 &&
		G_IS_DIR_SEPARATOR(path_utf8[music_dir_utf8_length])
		? path_utf8 + music_dir_utf8_length + 1
		: path_utf8;
}

Path
map_uri_fs(const char *uri)
{
	assert(uri != NULL);
	assert(*uri != '/');

	if (music_dir_fs.IsNull())
		return Path::Null();

	const Path uri_fs = Path::FromUTF8(uri);
	if (uri_fs.IsNull())
		return Path::Null();

	return Path::Build(music_dir_fs, uri_fs);
}

Path
map_directory_fs(const Directory *directory)
{
	assert(music_dir_utf8 != NULL);
	assert(!music_dir_fs.IsNull());

	if (directory->IsRoot())
		return music_dir_fs;

	return map_uri_fs(directory->GetPath());
}

Path
map_directory_child_fs(const Directory *directory, const char *name)
{
	assert(music_dir_utf8 != NULL);
	assert(!music_dir_fs.IsNull());

	/* check for invalid or unauthorized base names */
	if (*name == 0 || strchr(name, '/') != NULL ||
	    strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return Path::Null();

	const Path parent_fs = map_directory_fs(directory);
	if (parent_fs.IsNull())
		return Path::Null();

	const Path name_fs = Path::FromUTF8(name);
	if (name_fs.IsNull())
		return Path::Null();

	return Path::Build(parent_fs, name_fs);
}

/**
 * Map a song object that was created by song_dup_detached().  It does
 * not have a real parent directory, only the dummy object
 * #detached_root.
 */
static Path
map_detached_song_fs(const char *uri_utf8)
{
	Path uri_fs = Path::FromUTF8(uri_utf8);
	if (uri_fs.IsNull())
		return Path::Null();

	return Path::Build(music_dir_fs, uri_fs);
}

Path
map_song_fs(const Song *song)
{
	assert(song->IsFile());

	if (song->IsInDatabase())
		return song->IsDetached()
			? map_detached_song_fs(song->uri)
			: map_directory_child_fs(song->parent, song->uri);
	else
		return Path::FromUTF8(song->uri);
}

std::string
map_fs_to_utf8(const char *path_fs)
{
	if (!music_dir_fs.IsNull() &&
	    strncmp(path_fs, music_dir_fs.c_str(), music_dir_fs_length) == 0 &&
	    G_IS_DIR_SEPARATOR(path_fs[music_dir_fs_length]))
		/* remove musicDir prefix */
		path_fs += music_dir_fs_length + 1;
	else if (G_IS_DIR_SEPARATOR(path_fs[0]))
		/* not within musicDir */
		return NULL;

	while (path_fs[0] == G_DIR_SEPARATOR)
		++path_fs;

	return Path::ToUTF8(path_fs);
}

const Path &
map_spl_path(void)
{
	return playlist_dir_fs;
}

Path
map_spl_utf8_to_fs(const char *name)
{
	if (playlist_dir_fs.IsNull())
		return Path::Null();

	char *filename_utf8 = g_strconcat(name, PLAYLIST_FILE_SUFFIX, NULL);
	const Path filename_fs = Path::FromUTF8(filename_utf8);
	g_free(filename_utf8);
	if (filename_fs.IsNull())
		return Path::Null();

	return Path::Build(playlist_dir_fs, filename_fs);
}
