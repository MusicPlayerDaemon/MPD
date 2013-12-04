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
#include "Song.hxx"
#include "SongSave.hxx"
#include "PlaylistDatabase.hxx"
#include "TextFile.hxx"
#include "util/StringUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <stddef.h>

#define DIRECTORY_DIR "directory: "
#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

static constexpr Domain directory_domain("directory");

void
directory_save(FILE *fp, const Directory &directory)
{
	if (!directory.IsRoot()) {
		fprintf(fp, DIRECTORY_MTIME "%lu\n",
			(unsigned long)directory.mtime);

		fprintf(fp, "%s%s\n", DIRECTORY_BEGIN, directory.GetPath());
	}

	Directory *cur;
	directory_for_each_child(cur, directory) {
		fprintf(fp, DIRECTORY_DIR "%s\n", cur->GetName());

		directory_save(fp, *cur);

		if (ferror(fp))
			return;
	}

	Song *song;
	directory_for_each_song(song, directory)
		song_save(fp, *song);

	playlist_vector_save(fp, directory.playlists);

	if (!directory.IsRoot())
		fprintf(fp, DIRECTORY_END "%s\n", directory.GetPath());
}

static Directory *
directory_load_subdir(TextFile &file, Directory &parent, const char *name,
		      Error &error)
{
	bool success;

	if (parent.FindChild(name) != nullptr) {
		error.Format(directory_domain,
			     "Duplicate subdirectory '%s'", name);
		return nullptr;
	}

	Directory *directory = parent.CreateChild(name);

	const char *line = file.ReadLine();
	if (line == nullptr) {
		error.Set(directory_domain, "Unexpected end of file");
		directory->Delete();
		return nullptr;
	}

	if (StringStartsWith(line, DIRECTORY_MTIME)) {
		directory->mtime =
			ParseUint64(line + sizeof(DIRECTORY_MTIME) - 1);

		line = file.ReadLine();
		if (line == nullptr) {
			error.Set(directory_domain, "Unexpected end of file");
			directory->Delete();
			return nullptr;
		}
	}

	if (!StringStartsWith(line, DIRECTORY_BEGIN)) {
		error.Format(directory_domain, "Malformed line: %s", line);
		directory->Delete();
		return nullptr;
	}

	success = directory_load(file, *directory, error);
	if (!success) {
		directory->Delete();
		return nullptr;
	}

	return directory;
}

bool
directory_load(TextFile &file, Directory &directory, Error &error)
{
	const char *line;

	while ((line = file.ReadLine()) != nullptr &&
	       !StringStartsWith(line, DIRECTORY_END)) {
		if (StringStartsWith(line, DIRECTORY_DIR)) {
			Directory *subdir =
				directory_load_subdir(file, directory,
						      line + sizeof(DIRECTORY_DIR) - 1,
						      error);
			if (subdir == nullptr)
				return false;
		} else if (StringStartsWith(line, SONG_BEGIN)) {
			const char *name = line + sizeof(SONG_BEGIN) - 1;
			Song *song;

			if (directory.FindSong(name) != nullptr) {
				error.Format(directory_domain,
					     "Duplicate song '%s'", name);
				return false;
			}

			song = song_load(file, &directory, name, error);
			if (song == nullptr)
				return false;

			directory.AddSong(song);
		} else if (StringStartsWith(line, PLAYLIST_META_BEGIN)) {
			const char *name = line + sizeof(PLAYLIST_META_BEGIN) - 1;
			if (!playlist_metadata_load(file, directory.playlists,
						    name, error))
				return false;
		} else {
			error.Format(directory_domain,
				     "Malformed line: %s", line);
			return false;
		}
	}

	return true;
}
