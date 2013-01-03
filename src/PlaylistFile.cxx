/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "DatabasePlugin.hxx"
#include "DatabaseGlue.hxx"
#include "song.h"
#include "io_error.h"
#include "Mapper.hxx"
#include "TextFile.hxx"

extern "C" {
#include "path.h"
#include "uri.h"
#include "idle.h"
#include "conf.h"
}

#include "glib_compat.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
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

	return strchr(name_utf8, '/') == NULL &&
		strchr(name_utf8, '\n') == NULL &&
		strchr(name_utf8, '\r') == NULL;
}

static const char *
spl_map(GError **error_r)
{
	const char *path_fs = map_spl_path();
	if (path_fs == NULL)
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_DISABLED,
				    "Stored playlists are disabled");

	return path_fs;
}

static bool
spl_check_name(const char *name_utf8, GError **error_r)
{
	if (!spl_valid_name(name_utf8)) {
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_NAME,
				    "Bad playlist name");
		return false;
	}

	return true;
}

static char *
spl_map_to_fs(const char *name_utf8, GError **error_r)
{
	if (spl_map(error_r) == NULL ||
	    !spl_check_name(name_utf8, error_r))
		return NULL;

	char *path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs == NULL)
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_NAME,
				    "Bad playlist name");

	return path_fs;
}

/**
 * Create a GError for the current errno.
 */
static void
playlist_errno(GError **error_r)
{
	switch (errno) {
	case ENOENT:
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_NO_SUCH_LIST,
				    "No such playlist");
		break;

	default:
		set_error_errno(error_r);
		break;
	}
}

static bool
LoadPlaylistFileInfo(PlaylistFileInfo &info,
		     const char *parent_path_fs, const char *name_fs)
{
	size_t name_length = strlen(name_fs);

	if (name_length < sizeof(PLAYLIST_FILE_SUFFIX) ||
	    memchr(name_fs, '\n', name_length) != NULL)
		return false;

	if (!g_str_has_suffix(name_fs, PLAYLIST_FILE_SUFFIX))
		return false;

	char *path_fs = g_build_filename(parent_path_fs, name_fs, NULL);
	struct stat st;
	int ret = stat(path_fs, &st);
	g_free(path_fs);
	if (ret < 0 || !S_ISREG(st.st_mode))
		return false;

	char *name = g_strndup(name_fs,
			       name_length + 1 - sizeof(PLAYLIST_FILE_SUFFIX));
	char *name_utf8 = fs_charset_to_utf8(name);
	g_free(name);
	if (name_utf8 == NULL)
		return false;

	info.name = name_utf8;
	g_free(name_utf8);
	info.mtime = st.st_mtime;
	return true;
}

PlaylistFileList
ListPlaylistFiles(GError **error_r)
{
	PlaylistFileList list;

	const char *parent_path_fs = spl_map(error_r);
	if (parent_path_fs == NULL)
		return list;

	DIR *dir = opendir(parent_path_fs);
	if (dir == NULL) {
		set_error_errno(error_r);
		return list;
	}

	PlaylistFileInfo info;
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (LoadPlaylistFileInfo(info, parent_path_fs, ent->d_name))
			list.push_back(std::move(info));
	}

	closedir(dir);
	return list;
}

static bool
SavePlaylistFile(const PlaylistFileContents &contents, const char *utf8path,
		 GError **error_r)
{
	assert(utf8path != NULL);

	if (spl_map(error_r) == NULL)
		return false;

	char *path_fs = spl_map_to_fs(utf8path, error_r);
	if (path_fs == NULL)
		return false;

	FILE *file = fopen(path_fs, "w");
	g_free(path_fs);
	if (file == NULL) {
		playlist_errno(error_r);
		return false;
	}

	for (const auto &uri_utf8 : contents)
		playlist_print_uri(file, uri_utf8.c_str());

	fclose(file);
	return true;
}

PlaylistFileContents
LoadPlaylistFile(const char *utf8path, GError **error_r)
{
	PlaylistFileContents contents;

	if (spl_map(error_r) == NULL)
		return contents;

	char *path_fs = spl_map_to_fs(utf8path, error_r);
	if (path_fs == NULL)
		return contents;

	TextFile file(path_fs);
	if (file.HasFailed()) {
		playlist_errno(error_r);
		return contents;
	}

	char *s;
	while ((s = file.ReadLine()) != NULL) {
		if (*s == 0 || *s == PLAYLIST_COMMENT)
			continue;

		if (!uri_has_scheme(s)) {
			char *path_utf8;

			path_utf8 = map_fs_to_utf8(s);
			if (path_utf8 == NULL)
				continue;

			s = path_utf8;
		} else
			s = g_strdup(s);

		contents.emplace_back(s);
		if (contents.size() >= playlist_max_length)
			break;
	}

	return contents;
}

bool
spl_move_index(const char *utf8path, unsigned src, unsigned dest,
	       GError **error_r)
{
	if (src == dest)
		/* this doesn't check whether the playlist exists, but
		   what the hell.. */
		return true;

	GError *error = nullptr;
	auto contents = LoadPlaylistFile(utf8path, &error);
	if (contents.empty() && error != nullptr) {
		g_propagate_error(error_r, error);
		return false;
	}

	if (src >= contents.size() || dest >= contents.size()) {
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_RANGE,
				    "Bad range");
		return false;
	}

	const auto src_i = std::next(contents.begin(), src);
	auto value = std::move(*src_i);
	contents.erase(src_i);

	const auto dest_i = std::next(contents.begin(), dest);
	contents.insert(dest_i, std::move(value));

	bool result = SavePlaylistFile(contents, utf8path, error_r);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

bool
spl_clear(const char *utf8path, GError **error_r)
{
	FILE *file;

	if (spl_map(error_r) == NULL)
		return false;

	char *path_fs = spl_map_to_fs(utf8path, error_r);
	if (path_fs == NULL)
		return false;

	file = fopen(path_fs, "w");
	g_free(path_fs);
	if (file == NULL) {
		playlist_errno(error_r);
		return false;
	}

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_delete(const char *name_utf8, GError **error_r)
{
	char *path_fs = spl_map_to_fs(name_utf8, error_r);
	if (path_fs == NULL)
		return false;

	int ret = unlink(path_fs);
	g_free(path_fs);
	if (ret < 0) {
		playlist_errno(error_r);
		return false;
	}

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_remove_index(const char *utf8path, unsigned pos, GError **error_r)
{
	GError *error = nullptr;
	auto contents = LoadPlaylistFile(utf8path, &error);
	if (contents.empty() && error != nullptr) {
		g_propagate_error(error_r, error);
		return false;
	}

	if (pos >= contents.size()) {
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_RANGE,
				    "Bad range");
		return false;
	}

	contents.erase(std::next(contents.begin(), pos));

	bool result = SavePlaylistFile(contents, utf8path, error_r);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

bool
spl_append_song(const char *utf8path, struct song *song, GError **error_r)
{
	FILE *file;

	if (spl_map(error_r) == NULL)
		return false;

	char *path_fs = spl_map_to_fs(utf8path, error_r);
	if (path_fs == NULL)
		return false;

	file = fopen(path_fs, "a");
	g_free(path_fs);
	if (file == NULL) {
		playlist_errno(error_r);
		return false;
	}

	struct stat st;
	if (fstat(fileno(file), &st) < 0) {
		playlist_errno(error_r);
		fclose(file);
		return false;
	}

	if (st.st_size / (MPD_PATH_MAX + 1) >= (off_t)playlist_max_length) {
		fclose(file);
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_TOO_LARGE,
				    "Stored playlist is too large");
		return false;
	}

	playlist_print_song(file, song);

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_append_uri(const char *url, const char *utf8file, GError **error_r)
{
	if (uri_has_scheme(url)) {
		struct song *song = song_remote_new(url);
		bool success = spl_append_song(utf8file, song, error_r);
		song_free(song);
		return success;
	} else {
		const Database *db = GetDatabase(error_r);
		if (db == nullptr)
			return false;

		song *song = db->GetSong(url, error_r);
		if (song == nullptr)
			return false;

		bool success = spl_append_song(utf8file, song, error_r);
		db->ReturnSong(song);
		return success;
	}
}

static bool
spl_rename_internal(const char *from_path_fs, const char *to_path_fs,
		    GError **error_r)
{
	if (!g_file_test(from_path_fs, G_FILE_TEST_IS_REGULAR)) {
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_NO_SUCH_LIST,
				    "No such playlist");
		return false;
	}

	if (g_file_test(to_path_fs, G_FILE_TEST_EXISTS)) {
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_LIST_EXISTS,
				    "Playlist exists already");
		return false;
	}

	if (rename(from_path_fs, to_path_fs) < 0) {
		playlist_errno(error_r);
		return false;
	}

	idle_add(IDLE_STORED_PLAYLIST);
	return true;
}

bool
spl_rename(const char *utf8from, const char *utf8to, GError **error_r)
{
	if (spl_map(error_r) == NULL)
		return false;

	char *from_path_fs = spl_map_to_fs(utf8from, error_r);
	if (from_path_fs == NULL)
		return false;

	char *to_path_fs = spl_map_to_fs(utf8to, error_r);
	if (to_path_fs == NULL) {
		g_free(from_path_fs);
		return false;
	}

	bool success = spl_rename_internal(from_path_fs, to_path_fs, error_r);

	g_free(from_path_fs);
	g_free(to_path_fs);

	return success;
}
