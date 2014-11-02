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

#include "config.h"
#include "PlaylistFile.hxx"
#include "PlaylistSave.hxx"
#include "db/PlaylistInfo.hxx"
#include "db/PlaylistVector.hxx"
#include "DetachedSong.hxx"
#include "SongLoader.hxx"
#include "Mapper.hxx"
#include "fs/io/TextFile.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "config/ConfigDefaults.hxx"
#include "Idle.hxx"
#include "fs/Limits.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/Charset.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/StringUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

static const char PLAYLIST_COMMENT = '#';

static unsigned playlist_max_length;
bool playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

void
spl_global_init(void)
{
	playlist_max_length = config_get_positive(CONF_MAX_PLAYLIST_LENGTH,
						  DEFAULT_PLAYLIST_MAX_LENGTH);

	playlist_saveAbsolutePaths =
		config_get_bool(CONF_SAVE_ABSOLUTE_PATHS,
				DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS);
}

bool
spl_valid_name(const char *name_utf8)
{
	if (*name_utf8 == 0)
		/* empty name not allowed */
		return false;

	/*
	 * Not supporting '/' was done out of laziness, and we should
	 * really strive to support it in the future.
	 *
	 * Not supporting '\r' and '\n' is done out of protocol
	 * limitations (and arguably laziness), but bending over head
	 * over heels to modify the protocol (and compatibility with
	 * all clients) to support idiots who put '\r' and '\n' in
	 * filenames isn't going to happen, either.
	 */

	return strchr(name_utf8, '/') == nullptr &&
		strchr(name_utf8, '\n') == nullptr &&
		strchr(name_utf8, '\r') == nullptr;
}

static const AllocatedPath &
spl_map(Error &error)
{
	const AllocatedPath &path_fs = map_spl_path();
	if (path_fs.IsNull())
		error.Set(playlist_domain, int(PlaylistResult::DISABLED),
			  "Stored playlists are disabled");
	return path_fs;
}

static bool
spl_check_name(const char *name_utf8, Error &error)
{
	if (!spl_valid_name(name_utf8)) {
		error.Set(playlist_domain, int(PlaylistResult::BAD_NAME),
				    "Bad playlist name");
		return false;
	}

	return true;
}

static AllocatedPath
spl_map_to_fs(const char *name_utf8, Error &error)
{
	if (spl_map(error).IsNull() || !spl_check_name(name_utf8, error))
		return AllocatedPath::Null();

	auto path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs.IsNull())
		error.Set(playlist_domain, int(PlaylistResult::BAD_NAME),
			  "Bad playlist name");

	return path_fs;
}

gcc_pure
static bool
IsNotFoundError(const Error &error)
{
#ifdef WIN32
	return error.IsDomain(win32_domain) &&
		error.GetCode() == ERROR_FILE_NOT_FOUND;
#else
	return error.IsDomain(errno_domain) &&
		error.GetCode() == ENOENT;
#endif
}

static void
TranslatePlaylistError(Error &error)
{
	if (IsNotFoundError(error)) {
		error.Clear();
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_LIST),
			  "No such playlist");
	}
}

/**
 * Create an #Error for the current errno.
 */
static void
playlist_errno(Error &error)
{
	switch (errno) {
	case ENOENT:
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_LIST),
			  "No such playlist");
		break;

	default:
		error.SetErrno();
		break;
	}
}

static bool
LoadPlaylistFileInfo(PlaylistInfo &info,
		     const Path parent_path_fs,
		     const Path name_fs)
{
	const char *name_fs_str = name_fs.c_str();
	size_t name_length = strlen(name_fs_str);

	if (name_length < sizeof(PLAYLIST_FILE_SUFFIX) ||
	    memchr(name_fs_str, '\n', name_length) != nullptr)
		return false;

	if (!StringEndsWith(name_fs_str, PLAYLIST_FILE_SUFFIX))
		return false;

	const auto path_fs = AllocatedPath::Build(parent_path_fs, name_fs);
	struct stat st;
	if (!StatFile(path_fs, st) || !S_ISREG(st.st_mode))
		return false;

	std::string name(name_fs_str,
			 name_length + 1 - sizeof(PLAYLIST_FILE_SUFFIX));
	std::string name_utf8 = PathToUTF8(name.c_str());
	if (name_utf8.empty())
		return false;

	info.name = std::move(name_utf8);
	info.mtime = st.st_mtime;
	return true;
}

PlaylistVector
ListPlaylistFiles(Error &error)
{
	PlaylistVector list;

	const auto &parent_path_fs = spl_map(error);
	if (parent_path_fs.IsNull())
		return list;

	DirectoryReader reader(parent_path_fs);
	if (reader.HasFailed()) {
		error.SetErrno();
		return list;
	}

	PlaylistInfo info;
	while (reader.ReadEntry()) {
		const auto entry = reader.GetEntry();
		if (LoadPlaylistFileInfo(info, parent_path_fs, entry))
			list.push_back(std::move(info));
	}

	return list;
}

static bool
SavePlaylistFile(const PlaylistFileContents &contents, const char *utf8path,
		 Error &error)
{
	assert(utf8path != nullptr);

	if (spl_map(error).IsNull())
		return false;

	const auto path_fs = spl_map_to_fs(utf8path, error);
	if (path_fs.IsNull())
		return false;

	FILE *file = FOpen(path_fs, FOpenMode::WriteText);
	if (file == nullptr) {
		playlist_errno(error);
		return false;
	}

	for (const auto &uri_utf8 : contents)
		playlist_print_uri(file, uri_utf8.c_str());

	fclose(file);
	return true;
}

PlaylistFileContents
LoadPlaylistFile(const char *utf8path, Error &error)
{
	PlaylistFileContents contents;

	if (spl_map(error).IsNull())
		return contents;

	const auto path_fs = spl_map_to_fs(utf8path, error);
	if (path_fs.IsNull())
		return contents;

	TextFile file(path_fs, error);
	if (file.HasFailed()) {
		TranslatePlaylistError(error);
		return contents;
	}

	char *s;
	while ((s = file.ReadLine()) != nullptr) {
		if (*s == 0 || *s == PLAYLIST_COMMENT)
			continue;

		std::string uri_utf8;

		if (!uri_has_scheme(s)) {
#ifdef ENABLE_DATABASE
			uri_utf8 = map_fs_to_utf8(s);
			if (uri_utf8.empty()) {
				if (PathTraitsFS::IsAbsolute(s)) {
					uri_utf8 = PathToUTF8(s);
					if (uri_utf8.empty())
						continue;

					uri_utf8.insert(0, "file://");
				} else
					continue;
			}
#else
			continue;
#endif
		} else {
			uri_utf8 = PathToUTF8(s);
			if (uri_utf8.empty())
				continue;
		}

		contents.emplace_back(std::move(uri_utf8));
		if (contents.size() >= playlist_max_length)
			break;
	}

	return contents;
}

bool
spl_move_index(const char *utf8path, unsigned src, unsigned dest,
	       Error &error)
{
	if (src == dest)
		/* this doesn't check whether the playlist exists, but
		   what the hell.. */
		return true;

	auto contents = LoadPlaylistFile(utf8path, error);
	if (contents.empty() && error.IsDefined())
		return false;

	if (src >= contents.size() || dest >= contents.size()) {
		error.Set(playlist_domain, int(PlaylistResult::BAD_RANGE),
			  "Bad range");
		return false;
	}

	const auto src_i = std::next(contents.begin(), src);
	auto value = std::move(*src_i);
	contents.erase(src_i);

	const auto dest_i = std::next(contents.begin(), dest);
	contents.insert(dest_i, std::move(value));

	bool result = SavePlaylistFile(contents, utf8path, error);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

bool
spl_clear(const char *utf8path, Error &error)
{
	if (spl_map(error).IsNull())
		return false;

	const auto path_fs = spl_map_to_fs(utf8path, error);
	if (path_fs.IsNull())
		return false;

	FILE *file = FOpen(path_fs, FOpenMode::WriteText);
	if (file == nullptr) {
		playlist_errno(error);
		return false;
	}

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_delete(const char *name_utf8, Error &error)
{
	const auto path_fs = spl_map_to_fs(name_utf8, error);
	if (path_fs.IsNull())
		return false;

	if (!RemoveFile(path_fs)) {
		playlist_errno(error);
		return false;
	}

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_remove_index(const char *utf8path, unsigned pos, Error &error)
{
	auto contents = LoadPlaylistFile(utf8path, error);
	if (contents.empty() && error.IsDefined())
		return false;

	if (pos >= contents.size()) {
		error.Set(playlist_domain, int(PlaylistResult::BAD_RANGE),
			  "Bad range");
		return false;
	}

	contents.erase(std::next(contents.begin(), pos));

	bool result = SavePlaylistFile(contents, utf8path, error);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

bool
spl_append_song(const char *utf8path, const DetachedSong &song, Error &error)
{
	if (spl_map(error).IsNull())
		return false;

	const auto path_fs = spl_map_to_fs(utf8path, error);
	if (path_fs.IsNull())
		return false;

	FILE *file = FOpen(path_fs, FOpenMode::AppendText);
	if (file == nullptr) {
		playlist_errno(error);
		return false;
	}

	struct stat st;
	if (fstat(fileno(file), &st) < 0) {
		playlist_errno(error);
		fclose(file);
		return false;
	}

	if (st.st_size / off_t(MPD_PATH_MAX + 1) >= (off_t)playlist_max_length) {
		fclose(file);
		error.Set(playlist_domain, int(PlaylistResult::TOO_LARGE),
			  "Stored playlist is too large");
		return false;
	}

	playlist_print_song(file, song);

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_append_uri(const char *utf8file,
	       const SongLoader &loader, const char *url,
	       Error &error)
{
	DetachedSong *song = loader.LoadSong(url, error);
	if (song == nullptr)
		return false;

	bool success = spl_append_song(utf8file, *song, error);
	delete song;
	return success;
}

static bool
spl_rename_internal(Path from_path_fs, Path to_path_fs,
		    Error &error)
{
	if (!FileExists(from_path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_LIST),
			  "No such playlist");
		return false;
	}

	if (FileExists(to_path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::LIST_EXISTS),
			  "Playlist exists already");
		return false;
	}

	if (!RenameFile(from_path_fs, to_path_fs)) {
		playlist_errno(error);
		return false;
	}

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_rename(const char *utf8from, const char *utf8to, Error &error)
{
	if (spl_map(error).IsNull())
		return false;

	const auto from_path_fs = spl_map_to_fs(utf8from, error);
	if (from_path_fs.IsNull())
		return false;

	const auto to_path_fs = spl_map_to_fs(utf8to, error);
	if (to_path_fs.IsNull())
		return false;

	return spl_rename_internal(from_path_fs, to_path_fs, error);
}
