/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "song.h"
#include "mapper.h"
#include "path.h"
#include "uri.h"
#include "database.h"
#include "idle.h"
#include "conf.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

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
spl_list(void)
{
	const char *parent_path_fs = map_spl_path();
	DIR *dir;
	struct dirent *ent;
	GPtrArray *list;
	struct stored_playlist_info *playlist;

	if (parent_path_fs == NULL)
		return NULL;

	dir = opendir(parent_path_fs);
	if (dir == NULL)
		return NULL;

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

static enum playlist_result
spl_save(GPtrArray *list, const char *utf8path)
{
	FILE *file;
	char *path_fs;

	assert(utf8path != NULL);

	path_fs = map_spl_utf8_to_fs(utf8path);
	if (path_fs == NULL)
		return PLAYLIST_RESULT_DISABLED;

	while (!(file = fopen(path_fs, "w")) && errno == EINTR);
	g_free(path_fs);
	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	for (unsigned i = 0; i < list->len; ++i) {
		const char *uri = g_ptr_array_index(list, i);
		playlist_print_uri(file, uri);
	}

	while (fclose(file) != 0 && errno == EINTR);
	return PLAYLIST_RESULT_SUCCESS;
}

GPtrArray *
spl_load(const char *utf8path)
{
	FILE *file;
	GPtrArray *list;
	char buffer[MPD_PATH_MAX];
	char *path_fs;

	if (!spl_valid_name(utf8path))
		return NULL;

	path_fs = map_spl_utf8_to_fs(utf8path);
	if (path_fs == NULL)
		return NULL;

	while (!(file = fopen(path_fs, "r")) && errno == EINTR);
	g_free(path_fs);
	if (file == NULL)
		return NULL;

	list = g_ptr_array_new();

	while (fgets(buffer, sizeof(buffer), file)) {
		char *s = buffer;

		if (*s == PLAYLIST_COMMENT)
			continue;

		g_strchomp(buffer);

		if (!uri_has_scheme(s)) {
			char *path_utf8;
			struct song *song;

			path_utf8 = map_fs_to_utf8(s);
			if (path_utf8 == NULL)
				continue;

			song = db_get_song(path_utf8);
			g_free(path_utf8);
			if (song == NULL)
				continue;

			s = song_get_uri(song);
		} else
			s = g_strdup(s);

		g_ptr_array_add(list, s);

		if (list->len >= playlist_max_length)
			break;
	}

	while (fclose(file) && errno == EINTR);
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

enum playlist_result
spl_move_index(const char *utf8path, unsigned src, unsigned dest)
{
	GPtrArray *list;
	char *uri;
	enum playlist_result result;

	if (src == dest)
		/* this doesn't check whether the playlist exists, but
		   what the hell.. */
		return PLAYLIST_RESULT_SUCCESS;

	if (!(list = spl_load(utf8path)))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (src >= list->len || dest >= list->len) {
		spl_free(list);
		return PLAYLIST_RESULT_BAD_RANGE;
	}

	uri = spl_remove_index_internal(list, src);
	spl_insert_index_internal(list, dest, uri);

	result = spl_save(list, utf8path);

	spl_free(list);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

enum playlist_result
spl_clear(const char *utf8path)
{
	char *path_fs;
	FILE *file;

	if (!spl_valid_name(utf8path))
		return PLAYLIST_RESULT_BAD_NAME;

	path_fs = map_spl_utf8_to_fs(utf8path);
	if (path_fs == NULL)
		return PLAYLIST_RESULT_DISABLED;

	while (!(file = fopen(path_fs, "w")) && errno == EINTR);
	g_free(path_fs);
	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	while (fclose(file) != 0 && errno == EINTR);

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
spl_delete(const char *name_utf8)
{
	char *path_fs;
	int ret;

	path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs == NULL)
		return PLAYLIST_RESULT_DISABLED;

	ret = unlink(path_fs);
	g_free(path_fs);
	if (ret < 0)
		return errno == ENOENT
			? PLAYLIST_RESULT_NO_SUCH_LIST
			: PLAYLIST_RESULT_ERRNO;

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
spl_remove_index(const char *utf8path, unsigned pos)
{
	GPtrArray *list;
	char *uri;
	enum playlist_result result;

	if (!(list = spl_load(utf8path)))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (pos >= list->len) {
		spl_free(list);
		return PLAYLIST_RESULT_BAD_RANGE;
	}

	uri = spl_remove_index_internal(list, pos);
	g_free(uri);
	result = spl_save(list, utf8path);

	spl_free(list);

	idle_add(IDLE_STORED_PLAYLIST);
	return result;
}

enum playlist_result
spl_append_song(const char *utf8path, struct song *song)
{
	FILE *file;
	struct stat st;
	char *path_fs;

	if (!spl_valid_name(utf8path))
		return PLAYLIST_RESULT_BAD_NAME;

	path_fs = map_spl_utf8_to_fs(utf8path);
	if (path_fs == NULL)
		return PLAYLIST_RESULT_DISABLED;

	while (!(file = fopen(path_fs, "a")) && errno == EINTR);
	g_free(path_fs);
	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	if (fstat(fileno(file), &st) < 0) {
		int save_errno = errno;
		while (fclose(file) != 0 && errno == EINTR);
		errno = save_errno;
		return PLAYLIST_RESULT_ERRNO;
	}

	if (st.st_size / (MPD_PATH_MAX + 1) >= (off_t)playlist_max_length) {
		while (fclose(file) != 0 && errno == EINTR);
		return PLAYLIST_RESULT_TOO_LARGE;
	}

	playlist_print_song(file, song);

	while (fclose(file) != 0 && errno == EINTR);

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
spl_append_uri(const char *url, const char *utf8file)
{
	struct song *song;

	if (uri_has_scheme(url)) {
		enum playlist_result ret;

		song = song_remote_new(url);
		ret = spl_append_song(utf8file, song);
		song_free(song);
		return ret;
	} else {
		song = db_get_song(url);
		if (song == NULL)
			return PLAYLIST_RESULT_NO_SUCH_SONG;

		return spl_append_song(utf8file, song);
	}
}

static enum playlist_result
spl_rename_internal(const char *from_path_fs, const char *to_path_fs)
{
	if (!g_file_test(from_path_fs, G_FILE_TEST_IS_REGULAR))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	if (g_file_test(to_path_fs, G_FILE_TEST_EXISTS))
		return PLAYLIST_RESULT_LIST_EXISTS;

	if (rename(from_path_fs, to_path_fs) < 0)
		return PLAYLIST_RESULT_ERRNO;

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
spl_rename(const char *utf8from, const char *utf8to)
{
	char *from_path_fs, *to_path_fs;
	static enum playlist_result ret;

	if (!spl_valid_name(utf8from) || !spl_valid_name(utf8to))
		return PLAYLIST_RESULT_BAD_NAME;

	from_path_fs = map_spl_utf8_to_fs(utf8from);
	to_path_fs = map_spl_utf8_to_fs(utf8to);

	if (from_path_fs != NULL && to_path_fs != NULL)
		ret = spl_rename_internal(from_path_fs, to_path_fs);
	else
		ret = PLAYLIST_RESULT_DISABLED;

	g_free(from_path_fs);
	g_free(to_path_fs);

	return ret;
}
