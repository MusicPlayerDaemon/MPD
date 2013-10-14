/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "stored_playlist.h"
#include "playlist_save.h"
#include "text_file.h"
#include "song.h"
#include "mapper.h"
#include "path.h"
#include "uri.h"
#include "database.h"
#include "idle.h"
#include "conf.h"
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
		g_set_error_literal(error_r, g_file_error_quark(), errno,
				    g_strerror(errno));
		break;
	}
}

static struct stored_playlist_info *
load_playlist_info(const char *parent_path_fs, const char *name_fs)
{
	size_t name_length = strlen(name_fs);
	char *path_fs, *name, *name_utf8;
	int ret;
	struct stat st;
	struct stored_playlist_info *playlist;

	if (name_length < sizeof(PLAYLIST_FILE_SUFFIX) ||
	    memchr(name_fs, '\n', name_length) != NULL)
		return NULL;

	if (!g_str_has_suffix(name_fs, PLAYLIST_FILE_SUFFIX))
		return NULL;

	path_fs = g_build_filename(parent_path_fs, name_fs, NULL);
	ret = stat(path_fs, &st);
	g_free(path_fs);
	if (ret < 0 || !S_ISREG(st.st_mode))
		return NULL;

	name = g_strndup(name_fs,
			 name_length + 1 - sizeof(PLAYLIST_FILE_SUFFIX));
	name_utf8 = fs_charset_to_utf8(name);
	g_free(name);
	if (name_utf8 == NULL)
		return NULL;

	playlist = g_new(struct stored_playlist_info, 1);
	playlist->name = name_utf8;
	playlist->mtime = st.st_mtime;
	return playlist;
}

GPtrArray *
spl_list(GError **error_r)
{
	const char *parent_path_fs = spl_map(error_r);
	DIR *dir;
	struct dirent *ent;
	GPtrArray *list;
	struct stored_playlist_info *playlist;

	if (parent_path_fs == NULL)
		return NULL;

	dir = opendir(parent_path_fs);
	if (dir == NULL) {
		g_set_error_literal(error_r, g_file_error_quark(), errno,
				    g_strerror(errno));
		return NULL;
	}

	list = g_ptr_array_new();

	while ((ent = readdir(dir)) != NULL) {
		playlist = load_playlist_info(parent_path_fs, ent->d_name);
		if (playlist != NULL)
			g_ptr_array_add(list, playlist);
	}

	closedir(dir);
	return list;
}

void
spl_list_free(GPtrArray *list)
{
	for (unsigned i = 0; i < list->len; ++i) {
		struct stored_playlist_info *playlist =
			g_ptr_array_index(list, i);
		g_free(playlist->name);
		g_free(playlist);
	}

	g_ptr_array_free(list, true);
}

static bool
spl_save(GPtrArray *list, const char *utf8path, GError **error_r)
{
	FILE *file;

	assert(utf8path != NULL);

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

	for (unsigned i = 0; i < list->len; ++i) {
		const char *uri = g_ptr_array_index(list, i);
		playlist_print_uri(file, uri);
	}

	fclose(file);
	return true;
}

GPtrArray *
spl_load(const char *utf8path, GError **error_r)
{
	FILE *file;
	GPtrArray *list;
	char *path_fs;

	if (spl_map(error_r) == NULL)
		return NULL;

	path_fs = spl_map_to_fs(utf8path, error_r);
	if (path_fs == NULL)
		return NULL;

	file = fopen(path_fs, "r");
	g_free(path_fs);
	if (file == NULL) {
		playlist_errno(error_r);
		return NULL;
	}

	list = g_ptr_array_new();

	GString *buffer = g_string_sized_new(1024);
	char *s;
	while ((s = read_text_line(file, buffer)) != NULL) {
		if (*s == 0 || *s == PLAYLIST_COMMENT)
			continue;

		if (g_path_is_absolute(s)) {
			char *t = fs_charset_to_utf8(s);
			if (t == NULL)
				continue;

			s = g_strconcat("file://", t, NULL);
			g_free(t);
		} else if (!uri_has_scheme(s)) {
			char *path_utf8;

			path_utf8 = map_fs_to_utf8(s);
			if (path_utf8 == NULL)
				continue;

			s = path_utf8;
		} else {
			s = fs_charset_to_utf8(s);
			if (s == NULL)
				continue;
		}

		g_ptr_array_add(list, s);

		if (list->len >= playlist_max_length)
			break;
	}

	fclose(file);
	return list;
}

void
spl_free(GPtrArray *list)
{
	for (unsigned i = 0; i < list->len; ++i) {
		char *uri = g_ptr_array_index(list, i);
		g_free(uri);
	}

	g_ptr_array_free(list, true);
}

static char *
spl_remove_index_internal(GPtrArray *list, unsigned idx)
{
	char *uri;

	assert(idx < list->len);

	uri = g_ptr_array_remove_index(list, idx);
	assert(uri != NULL);
	return uri;
}

static void
spl_insert_index_internal(GPtrArray *list, unsigned idx, char *uri)
{
	assert(idx <= list->len);

	g_ptr_array_add(list, uri);

	memmove(list->pdata + idx + 1, list->pdata + idx,
		(list->len - idx - 1) * sizeof(list->pdata[0]));
	g_ptr_array_index(list, idx) = uri;
}

bool
spl_move_index(const char *utf8path, unsigned src, unsigned dest,
	       GError **error_r)
{
	char *uri;

	if (src == dest)
		/* this doesn't check whether the playlist exists, but
		   what the hell.. */
		return true;

	GPtrArray *list = spl_load(utf8path, error_r);
	if (list == NULL)
		return false;

	if (src >= list->len || dest >= list->len) {
		spl_free(list);
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_RANGE,
				    "Bad range");
		return false;
	}

	uri = spl_remove_index_internal(list, src);
	spl_insert_index_internal(list, dest, uri);

	bool result = spl_save(list, utf8path, error_r);

	spl_free(list);

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
	char *path_fs;
	int ret;

	path_fs = spl_map_to_fs(name_utf8, error_r);
	if (path_fs == NULL)
		return false;

	ret = unlink(path_fs);
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
	char *uri;

	GPtrArray *list = spl_load(utf8path, error_r);
	if (list == NULL)
		return false;

	if (pos >= list->len) {
		spl_free(list);
		g_set_error_literal(error_r, playlist_quark(),
				    PLAYLIST_RESULT_BAD_RANGE,
				    "Bad range");
		return false;
	}

	uri = spl_remove_index_internal(list, pos);
	g_free(uri);
	bool result = spl_save(list, utf8path, error_r);

	spl_free(list);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

bool
spl_append_song(const char *utf8path, struct song *song, GError **error_r)
{
	FILE *file;
	struct stat st;

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
	struct song *song;

	if (uri_has_scheme(url)) {
		song = song_remote_new(url);
		bool success = spl_append_song(utf8file, song, error_r);
		song_free(song);
		return success;
	} else {
		song = db_get_song(url);
		if (song == NULL) {
			g_set_error_literal(error_r, playlist_quark(),
					    PLAYLIST_RESULT_NO_SUCH_SONG,
					    "No such song");
			return false;
		}

		return spl_append_song(utf8file, song, error_r);
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
