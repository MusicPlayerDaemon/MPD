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

#ifndef MPD_SONG_HXX
#define MPD_SONG_HXX

#include "util/list.h"
#include "Compiler.h"

#include <string>

#include <assert.h>
#include <time.h>

#define SONG_FILE	"file: "
#define SONG_TIME	"Time: "

struct Tag;
struct Directory;
class DetachedSong;

/**
 * A song file inside the configured music directory.
 */
struct Song {
	/**
	 * Pointers to the siblings of this directory within the
	 * parent directory.  It is unused (undefined) if this song is
	 * not in the database.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	struct list_head siblings;

	Tag *tag;

	/**
	 * The #Directory that contains this song.  May be nullptr if
	 * the current database plugin does not manage the parent
	 * directory this way.
	 */
	Directory *parent;

	time_t mtime;

	/**
	 * Start of this sub-song within the file in milliseconds.
	 */
	unsigned start_ms;

	/**
	 * End of this sub-song within the file in milliseconds.
	 * Unused if zero.
	 */
	unsigned end_ms;

	/**
	 * The file name.  If #parent is nullptr, then this is the URI
	 * relative to the music directory.
	 */
	char uri[sizeof(int)];

	gcc_malloc
	static Song *NewFrom(DetachedSong &&other, Directory *parent);

	/** allocate a new song with a local file name */
	gcc_malloc
	static Song *NewFile(const char *path_utf8, Directory *parent);

	/**
	 * allocate a new song structure with a local file name and attempt to
	 * load its metadata.  If all decoder plugin fail to read its meta
	 * data, nullptr is returned.
	 */
	gcc_malloc
	static Song *LoadFile(const char *path_utf8, Directory *parent);

	static Song *LoadFile(const char *path_utf8, Directory &parent) {
		return LoadFile(path_utf8, &parent);
	}

	void Free();

	bool UpdateFile();
	bool UpdateFileInArchive();

	/**
	 * Returns the URI of the song in UTF-8 encoding, including its
	 * location within the music directory.
	 */
	gcc_pure
	std::string GetURI() const;

	gcc_pure
	double GetDuration() const;
};

#endif
