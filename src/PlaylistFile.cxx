/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "PlaylistError.hxx"
#include "db/PlaylistInfo.hxx"
#include "db/PlaylistVector.hxx"
#include "song/DetachedSong.hxx"
#include "SongLoader.hxx"
#include "Mapper.hxx"
#include "protocol/RangeArg.hxx"
#include "fs/io/TextFile.hxx"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "config/Defaults.hxx"
#include "config/QueueConfig.hxx"
#include "Idle.hxx"
#include "fs/Limits.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/FileInfo.hxx"
#include "fs/DirectoryReader.hxx"
#include "util/StringCompare.hxx"
#include "util/UriExtract.hxx"

#include <cassert>
#include <cstring>

static const char PLAYLIST_COMMENT = '#';

static unsigned playlist_max_length;
bool playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

void
spl_global_init(const ConfigData &config)
{
	playlist_max_length =
		config.GetPositive(ConfigOption::MAX_PLAYLIST_LENGTH,
				   QueueConfig::DEFAULT_MAX_LENGTH);

	playlist_saveAbsolutePaths =
		config.GetBool(ConfigOption::SAVE_ABSOLUTE_PATHS,
			       DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS);
}

bool
spl_valid_name(const char *name_utf8)
{
	if (StringIsEmpty(name_utf8))
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

	return std::strchr(name_utf8, '/') == nullptr &&
		std::strchr(name_utf8, '\n') == nullptr &&
		std::strchr(name_utf8, '\r') == nullptr;
}

static const AllocatedPath &
spl_map()
{
	const AllocatedPath &path_fs = map_spl_path();
	if (path_fs.IsNull())
		throw PlaylistError(PlaylistResult::DISABLED,
				    "Stored playlists are disabled");

	return path_fs;
}

static void
spl_check_name(const char *name_utf8)
{
	if (!spl_valid_name(name_utf8))
		throw PlaylistError(PlaylistResult::BAD_NAME,
				    "Bad playlist name");
}

AllocatedPath
spl_map_to_fs(const char *name_utf8)
{
	spl_map();
	spl_check_name(name_utf8);

	auto path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs.IsNull())
		throw PlaylistError(PlaylistResult::BAD_NAME,
				    "Bad playlist name");

	return path_fs;
}

static bool
LoadPlaylistFileInfo(PlaylistInfo &info,
		     const Path parent_path_fs,
		     const Path name_fs)
{
	if (name_fs.HasNewline())
		return false;

	const auto *const name_fs_str = name_fs.c_str();
	const auto *const name_fs_end =
		FindStringSuffix(name_fs_str,
				 PATH_LITERAL(PLAYLIST_FILE_SUFFIX));
	if (name_fs_end == nullptr ||
	    /* no empty playlist names (raw file name = ".m3u") */
	    name_fs_end == name_fs_str)
		return false;

	FileInfo fi;
	if (!GetFileInfo(parent_path_fs / name_fs, fi) ||
	    !fi.IsRegular())
		return false;

	const auto name = AllocatedPath::FromFS(name_fs_str, name_fs_end);

	try {
		info.name = name.ToUTF8Throw();
	} catch (...) {
		return false;
	}

	info.mtime = fi.GetModificationTime();
	return true;
}

PlaylistVector
ListPlaylistFiles()
{
	PlaylistVector list;

	const auto &parent_path_fs = spl_map();
	assert(!parent_path_fs.IsNull());

	DirectoryReader reader(parent_path_fs);

	PlaylistInfo info;
	while (reader.ReadEntry()) {
		const auto entry = reader.GetEntry();
		if (LoadPlaylistFileInfo(info, parent_path_fs, entry))
			list.push_back(std::move(info));
	}

	return list;
}

static void
SavePlaylistFile(Path path_fs, const PlaylistFileContents &contents)
{
	assert(!path_fs.IsNull());

	FileOutputStream fos(path_fs);
	BufferedOutputStream bos(fos);

	for (const auto &uri_utf8 : contents)
		playlist_print_uri(bos, uri_utf8.c_str());

	bos.Flush();

	fos.Commit();
}

static PlaylistFileContents
LoadPlaylistFile(Path path_fs)
try {
	PlaylistFileContents contents;

	assert(!path_fs.IsNull());

	TextFile file(path_fs);

	char *s;
	while ((s = file.ReadLine()) != nullptr) {
		if (*s == 0 || *s == PLAYLIST_COMMENT)
			continue;

#ifdef _UNICODE
		/* on Windows, playlists always contain UTF-8, because
		   its "narrow" charset (i.e. CP_ACP) is incapable of
		   storing all Unicode paths */
		const auto path = AllocatedPath::FromUTF8(s);
		if (path.IsNull())
			continue;
#else
		const Path path = Path::FromFS(s);
#endif

		std::string uri_utf8;

		if (!uri_has_scheme(s)) {
#ifdef ENABLE_DATABASE
			uri_utf8 = map_fs_to_utf8(path);
			if (uri_utf8.empty()) {
				if (path.IsAbsolute()) {
					uri_utf8 = path.ToUTF8();
					if (uri_utf8.empty())
						continue;
				} else
					continue;
			}
#else
			continue;
#endif
		} else {
			uri_utf8 = path.ToUTF8();
			if (uri_utf8.empty())
				continue;
		}

		contents.emplace_back(std::move(uri_utf8));
		if (contents.size() >= playlist_max_length)
			break;
	}

	return contents;
} catch (const std::system_error &e) {
	if (IsFileNotFound(e))
		throw PlaylistError::NoSuchList();
	throw;
}

static PlaylistFileContents
MaybeLoadPlaylistFile(Path path_fs, PlaylistFileEditor::LoadMode load_mode)
try {
	if (load_mode == PlaylistFileEditor::LoadMode::NO)
		return {};

	return LoadPlaylistFile(path_fs);
} catch (const PlaylistError &error) {
	if (error.GetCode() == PlaylistResult::NO_SUCH_LIST &&
	    load_mode == PlaylistFileEditor::LoadMode::TRY)
		return {};

	throw;
 }

PlaylistFileEditor::PlaylistFileEditor(const char *name_utf8,
				       LoadMode load_mode)
	:path(spl_map_to_fs(name_utf8)),
	 contents(MaybeLoadPlaylistFile(path, load_mode))
{
}

void
PlaylistFileEditor::Insert(std::size_t i, const char *uri)
{
	if (i > size())
		throw PlaylistError(PlaylistResult::BAD_RANGE, "Bad position");

	if (size() >= playlist_max_length)
		throw PlaylistError(PlaylistResult::TOO_LARGE,
				    "Stored playlist is too large");

	contents.emplace(std::next(contents.begin(), i), uri);
}

void
PlaylistFileEditor::Insert(std::size_t i, const DetachedSong &song)
{
	const char *uri = playlist_saveAbsolutePaths
		? song.GetRealURI()
		: song.GetURI();

	Insert(i, uri);
}

void
PlaylistFileEditor::MoveIndex(unsigned src, unsigned dest)
{
	if (src >= contents.size() || dest >= contents.size())
		throw PlaylistError(PlaylistResult::BAD_RANGE, "Bad range");

	const auto src_i = std::next(contents.begin(), src);
	auto value = std::move(*src_i);
	contents.erase(src_i);

	const auto dest_i = std::next(contents.begin(), dest);
	contents.insert(dest_i, std::move(value));
}

void
PlaylistFileEditor::RemoveIndex(unsigned i)
{
	if (i >= contents.size())
		throw PlaylistError(PlaylistResult::BAD_RANGE, "Bad range");

	contents.erase(std::next(contents.begin(), i));
}

void
PlaylistFileEditor::RemoveRange(RangeArg range)
{
	if (!range.CheckClip(size()))
		throw PlaylistError::BadRange();

	contents.erase(std::next(contents.begin(), range.start),
		       std::next(contents.begin(), range.end));
}

void
PlaylistFileEditor::Save()
{
	SavePlaylistFile(path, contents);
	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_clear(const char *utf8path)
{
	const auto path_fs = spl_map_to_fs(utf8path);
	assert(!path_fs.IsNull());

	try {
		TruncateFile(path_fs);
	} catch (const std::system_error &e) {
		if (IsFileNotFound(e))
			throw PlaylistError(PlaylistResult::NO_SUCH_LIST,
					    "No such playlist");
		else
			throw;
	}

	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_delete(const char *name_utf8)
{
	const auto path_fs = spl_map_to_fs(name_utf8);
	assert(!path_fs.IsNull());

	try {
		RemoveFile(path_fs);
	} catch (const std::system_error &e) {
		if (IsFileNotFound(e))
			throw PlaylistError(PlaylistResult::NO_SUCH_LIST,
					    "No such playlist");
		else
			throw;
	}

	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_append_song(const char *utf8path, const DetachedSong &song)
try {
	const auto path_fs = spl_map_to_fs(utf8path);
	assert(!path_fs.IsNull());

	FileOutputStream fos(path_fs, FileOutputStream::Mode::APPEND_OR_CREATE);

	if (fos.Tell() / (MPD_PATH_MAX + 1) >= playlist_max_length)
		throw PlaylistError(PlaylistResult::TOO_LARGE,
				    "Stored playlist is too large");

	BufferedOutputStream bos(fos);

	playlist_print_song(bos, song);

	bos.Flush();
	fos.Commit();

	idle_add(IDLE_STORED_PLAYLIST);
} catch (const std::system_error &e) {
	if (IsFileNotFound(e))
		throw PlaylistError::NoSuchList();
	throw;
}

void
spl_append_uri(const char *utf8file,
	       const SongLoader &loader, const char *url)
{
	spl_append_song(utf8file, loader.LoadSong(url));
}

static void
spl_rename_internal(Path from_path_fs, Path to_path_fs)
{
	if (FileExists(to_path_fs))
		throw PlaylistError(PlaylistResult::LIST_EXISTS,
				    "Playlist exists already");

	try {
		RenameFile(from_path_fs, to_path_fs);
	} catch (const std::system_error &e) {
		if (IsFileNotFound(e))
			throw PlaylistError(PlaylistResult::NO_SUCH_LIST,
					    "No such playlist");
		else
			throw;
	}

	idle_add(IDLE_STORED_PLAYLIST);
}

void
spl_rename(const char *utf8from, const char *utf8to)
{
	const auto from_path_fs = spl_map_to_fs(utf8from);
	assert(!from_path_fs.IsNull());

	const auto to_path_fs = spl_map_to_fs(utf8to);
	assert(!to_path_fs.IsNull());

	spl_rename_internal(from_path_fs, to_path_fs);
}
