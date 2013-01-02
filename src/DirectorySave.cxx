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
#include "DirectorySave.hxx"
#include "Directory.hxx"
#include "song.h"
#include "SongSave.hxx"
#include "PlaylistDatabase.hxx"

extern "C" {
#include "text_file.h"
}

#include <assert.h>
#include <string.h>

#define DIRECTORY_DIR "directory: "
#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

/**
 * The quark used for GError.domain.
 */
static inline GQuark
directory_quark(void)
{
	return g_quark_from_static_string("directory");
}

void
directory_save(FILE *fp, const Directory *directory)
{
	if (!directory->IsRoot()) {
		fprintf(fp, DIRECTORY_MTIME "%lu\n",
			(unsigned long)directory->mtime);

		fprintf(fp, "%s%s\n", DIRECTORY_BEGIN, directory->GetPath());
	}

	Directory *cur;
	directory_for_each_child(cur, directory) {
		char *base = g_path_get_basename(cur->path);

		fprintf(fp, DIRECTORY_DIR "%s\n", base);
		g_free(base);

		directory_save(fp, cur);

		if (ferror(fp))
			return;
	}

	struct song *song;
	directory_for_each_song(song, directory)
		song_save(fp, song);

	playlist_vector_save(fp, directory->playlists);

	if (!directory->IsRoot())
		fprintf(fp, DIRECTORY_END "%s\n", directory->GetPath());
}

static Directory *
directory_load_subdir(FILE *fp, Directory *parent, const char *name,
		      GString *buffer, GError **error_r)
{
	const char *line;
	bool success;

	if (parent->FindChild(name) != nullptr) {
		g_set_error(error_r, directory_quark(), 0,
			    "Duplicate subdirectory '%s'", name);
		return NULL;
	}

	Directory *directory = parent->CreateChild(name);

	line = read_text_line(fp, buffer);
	if (line == NULL) {
		g_set_error(error_r, directory_quark(), 0,
			    "Unexpected end of file");
		directory->Delete();
		return NULL;
	}

	if (g_str_has_prefix(line, DIRECTORY_MTIME)) {
		directory->mtime =
			g_ascii_strtoull(line + sizeof(DIRECTORY_MTIME) - 1,
					 NULL, 10);

		line = read_text_line(fp, buffer);
		if (line == NULL) {
			g_set_error(error_r, directory_quark(), 0,
				    "Unexpected end of file");
			directory->Delete();
			return NULL;
		}
	}

	if (!g_str_has_prefix(line, DIRECTORY_BEGIN)) {
		g_set_error(error_r, directory_quark(), 0,
			    "Malformed line: %s", line);
		directory->Delete();
		return NULL;
	}

	success = directory_load(fp, directory, buffer, error_r);
	if (!success) {
		directory->Delete();
		return NULL;
	}

	return directory;
}

bool
directory_load(FILE *fp, Directory *directory,
	       GString *buffer, GError **error)
{
	const char *line;

	while ((line = read_text_line(fp, buffer)) != NULL &&
	       !g_str_has_prefix(line, DIRECTORY_END)) {
		if (g_str_has_prefix(line, DIRECTORY_DIR)) {
			Directory *subdir =
				directory_load_subdir(fp, directory,
						      line + sizeof(DIRECTORY_DIR) - 1,
						      buffer, error);
			if (subdir == NULL)
				return false;
		} else if (g_str_has_prefix(line, SONG_BEGIN)) {
			const char *name = line + sizeof(SONG_BEGIN) - 1;
			struct song *song;

			if (directory->FindSong(name) != nullptr) {
				g_set_error(error, directory_quark(), 0,
					    "Duplicate song '%s'", name);
				return false;
			}

			song = song_load(fp, directory, name,
					 buffer, error);
			if (song == NULL)
				return false;

			directory->AddSong(song);
		} else if (g_str_has_prefix(line, PLAYLIST_META_BEGIN)) {
			/* duplicate the name, because
			   playlist_metadata_load() will overwrite the
			   buffer */
			char *name = g_strdup(line + sizeof(PLAYLIST_META_BEGIN) - 1);

			if (!playlist_metadata_load(fp, directory->playlists,
						    name, buffer, error)) {
				g_free(name);
				return false;
			}

			g_free(name);
		} else {
			g_set_error(error, directory_quark(), 0,
				    "Malformed line: %s", line);
			return false;
		}
	}

	return true;
}
