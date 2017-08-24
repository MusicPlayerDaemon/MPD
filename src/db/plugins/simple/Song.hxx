/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "check.h"
#include "Chrono.hxx"
#include "tag/Tag.hxx"
#include "Compiler.h"

#include <boost/intrusive/list.hpp>

#include <string>

#include <time.h>

struct LightSong;
struct Directory;
class DetachedSong;
class Storage;
class ArchiveFile;

/**
 * A song file inside the configured music directory.  Internal
 * #SimpleDatabase class.
 */
struct Song {
	static constexpr auto link_mode = boost::intrusive::normal_link;
	typedef boost::intrusive::link_mode<link_mode> LinkMode;
	typedef boost::intrusive::list_member_hook<LinkMode> Hook;

	struct Disposer {
		void operator()(Song *song) const {
			song->Free();
		}
	};

	/**
	 * Pointers to the siblings of this directory within the
	 * parent directory.  It is unused (undefined) if this song is
	 * not in the database.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	Hook siblings;

	Tag tag;

	/**
	 * The #Directory that contains this song.  Must be
	 * non-nullptr.  directory this way.
	 */
	Directory *const parent;

	/**
	 * The time stamp of the last file modification.  A negative
	 * value means that this is unknown/unavailable.
	 */
	std::chrono::system_clock::time_point mtime;

	/**
	 * Start of this sub-song within the file.
	 */
	SongTime start_time;

	/**
	 * End of this sub-song within the file.
	 * Unused if zero.
	 */
	SongTime end_time;

	/**
	 * The file name.
	 */
	char uri[sizeof(int)];

	Song(const char *_uri, size_t uri_length, Directory &parent);
	~Song();

	gcc_malloc
	static Song *NewFrom(DetachedSong &&other, Directory &parent);

	/** allocate a new song with a local file name */
	gcc_malloc
	static Song *NewFile(const char *path_utf8, Directory &parent);

	/**
	 * allocate a new song structure with a local file name and attempt to
	 * load its metadata.  If all decoder plugin fail to read its meta
	 * data, nullptr is returned.
	 */
	gcc_malloc
	static Song *LoadFile(Storage &storage, const char *name_utf8,
			      Directory &parent);

	void Free();

	bool UpdateFile(Storage &storage);

#ifdef ENABLE_ARCHIVE
	static Song *LoadFromArchive(ArchiveFile &archive,
				     const char *name_utf8,
				     Directory &parent);
	bool UpdateFileInArchive(ArchiveFile &archive);
#endif

	/**
	 * Returns the URI of the song in UTF-8 encoding, including its
	 * location within the music directory.
	 */
	gcc_pure
	std::string GetURI() const noexcept;

	gcc_pure
	LightSong Export() const noexcept;
};

typedef boost::intrusive::list<Song,
			       boost::intrusive::member_hook<Song, Song::Hook,
							     &Song::siblings>,
			       boost::intrusive::constant_time_size<false>> SongList;

#endif
