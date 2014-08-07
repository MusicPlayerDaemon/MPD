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
#include "DirectorySave.hxx"
#include "Directory.hxx"
#include "Song.hxx"
#include "SongSave.hxx"
#include "DetachedSong.hxx"
#include "PlaylistDatabase.hxx"
#include "fs/io/TextFile.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "util/StringUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <stddef.h>
#include <string.h>

#define DIRECTORY_DIR "directory: "
#define DIRECTORY_TYPE "type: "
#define DIRECTORY_MTIME "mtime: "
#define DIRECTORY_BEGIN "begin: "
#define DIRECTORY_END "end: "

static constexpr Domain directory_domain("directory");

gcc_const
static const char *
DeviceToTypeString(unsigned device)
{
	switch (device) {
	case DEVICE_INARCHIVE:
		return "archive";

	case DEVICE_CONTAINER:
		return "container";

	default:
		return nullptr;
	}
}

gcc_pure
static unsigned
ParseTypeString(const char *type)
{
	if (strcmp(type, "archive") == 0)
		return DEVICE_INARCHIVE;
	else if (strcmp(type, "container") == 0)
		return DEVICE_CONTAINER;
	else
		return 0;
}

void
directory_save(BufferedOutputStream &os, const Directory &directory)
{
	if (!directory.IsRoot()) {
		const char *type = DeviceToTypeString(directory.device);
		if (type != nullptr)
			os.Format(DIRECTORY_TYPE "%s\n", type);

		if (directory.mtime != 0)
			os.Format(DIRECTORY_MTIME "%lu\n",
				  (unsigned long)directory.mtime);

		os.Format("%s%s\n", DIRECTORY_BEGIN, directory.GetPath());
	}

	for (const auto &child : directory.children) {
		os.Format(DIRECTORY_DIR "%s\n", child.GetName());

		if (!child.IsMount())
			directory_save(os, child);

		if (!os.Check())
			return;
	}

	for (const auto &song : directory.songs)
		song_save(os, song);

	playlist_vector_save(os, directory.playlists);

	if (!directory.IsRoot())
		os.Format(DIRECTORY_END "%s\n", directory.GetPath());
}

static bool
ParseLine(Directory &directory, const char *line)
{
	if (StringStartsWith(line, DIRECTORY_MTIME)) {
		directory.mtime =
			ParseUint64(line + sizeof(DIRECTORY_MTIME) - 1);
	} else if (StringStartsWith(line, DIRECTORY_TYPE)) {
		directory.device =
			ParseTypeString(line + sizeof(DIRECTORY_TYPE) - 1);
	} else
		return false;

	return true;
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

	while (true) {
		const char *line = file.ReadLine();
		if (line == nullptr) {
			error.Set(directory_domain, "Unexpected end of file");
			directory->Delete();
			return nullptr;
		}

		if (StringStartsWith(line, DIRECTORY_BEGIN))
			break;

		if (!ParseLine(*directory, line)) {
			error.Format(directory_domain,
				     "Malformed line: %s", line);
			directory->Delete();
			return nullptr;
		}
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

			if (directory.FindSong(name) != nullptr) {
				error.Format(directory_domain,
					     "Duplicate song '%s'", name);
				return false;
			}

			DetachedSong *song = song_load(file, name, error);
			if (song == nullptr)
				return false;

			directory.AddSong(Song::NewFrom(std::move(*song),
							directory));
			delete song;
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
