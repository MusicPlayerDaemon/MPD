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

#include "config.h"
#include "PlaylistSave.hxx"
#include "PlaylistFile.hxx"
#include "Playlist.hxx"
#include "song.h"
#include "Mapper.hxx"
#include "Idle.hxx"
#include "Path.hxx"

extern "C" {
#include "uri.h"
}

#include "glib_compat.h"

#include <glib.h>

void
playlist_print_song(FILE *file, const struct song *song)
{
	if (playlist_saveAbsolutePaths && song_in_database(song)) {
		const Path path = map_song_fs(song);
		if (!path.IsNull())
			fprintf(file, "%s\n", path.c_str());
	} else {
		char *uri = song_get_uri(song);
		const Path uri_fs = Path::FromUTF8(uri);
		g_free(uri);

		fprintf(file, "%s\n", uri_fs.c_str());
	}
}

void
playlist_print_uri(FILE *file, const char *uri)
{
	Path path = playlist_saveAbsolutePaths && !uri_has_scheme(uri) &&
		!g_path_is_absolute(uri)
		? map_uri_fs(uri)
		: Path::FromUTF8(uri);

	if (!path.IsNull())
		fprintf(file, "%s\n", path.c_str());
}

enum playlist_result
spl_save_queue(const char *name_utf8, const struct queue *queue)
{
	if (map_spl_path() == NULL)
		return PLAYLIST_RESULT_DISABLED;

	if (!spl_valid_name(name_utf8))
		return PLAYLIST_RESULT_BAD_NAME;

	const Path path_fs = map_spl_utf8_to_fs(name_utf8);
	if (path_fs.IsNull())
		return PLAYLIST_RESULT_BAD_NAME;

	if (g_file_test(path_fs.c_str(), G_FILE_TEST_EXISTS))
		return PLAYLIST_RESULT_LIST_EXISTS;

	FILE *file = fopen(path_fs.c_str(), "w");

	if (file == NULL)
		return PLAYLIST_RESULT_ERRNO;

	for (unsigned i = 0; i < queue->GetLength(); i++)
		playlist_print_song(file, queue->Get(i));

	fclose(file);

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
spl_save_playlist(const char *name_utf8, const struct playlist *playlist)
{
	return spl_save_queue(name_utf8, &playlist->queue);
}

bool
playlist_load_spl(struct playlist *playlist, struct player_control *pc,
		  const char *name_utf8,
		  unsigned start_index, unsigned end_index,
		  GError **error_r)
{
	GError *error = NULL;
	PlaylistFileContents contents = LoadPlaylistFile(name_utf8, &error);
	if (contents.empty() && error != nullptr) {
		g_propagate_error(error_r, error);
		return false;
	}

	if (end_index > contents.size())
		end_index = contents.size();

	for (unsigned i = start_index; i < end_index; ++i) {
		const auto &uri_utf8 = contents[i];

		if ((playlist->AppendURI(*pc, uri_utf8.c_str())) != PLAYLIST_RESULT_SUCCESS) {
			/* for windows compatibility, convert slashes */
			char *temp2 = g_strdup(uri_utf8.c_str());
			char *p = temp2;
			while (*p) {
				if (*p == '\\')
					*p = '/';
				p++;
			}

			if (playlist->AppendURI(*pc, temp2) != PLAYLIST_RESULT_SUCCESS)
				g_warning("can't add file \"%s\"", temp2);

			g_free(temp2);
		}
	}

	return true;
}
