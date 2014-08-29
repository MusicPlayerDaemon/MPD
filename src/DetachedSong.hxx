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

#ifndef MPD_DETACHED_SONG_HXX
#define MPD_DETACHED_SONG_HXX

#include "check.h"
#include "tag/Tag.hxx"
#include "Chrono.hxx"
#include "Compiler.h"

#include <string>
#include <utility>

#include <time.h>

struct LightSong;
class Storage;

class DetachedSong {
	friend DetachedSong map_song_detach(const LightSong &song);
	friend DetachedSong DatabaseDetachSong(const Storage &db,
					       const LightSong &song);

	/**
	 * An UTF-8-encoded URI referring to the song file.  This can
	 * be one of:
	 *
	 * - an absolute URL with a scheme
	 *   (e.g. "http://example.com/foo.mp3")
	 *
	 * - an absolute file name
	 *
	 * - a file name relative to the music directory
	 */
	std::string uri;

	/**
	 * The "real" URI, the one to be used for opening the
	 * resource.  If this attribute is empty, then #uri shall be
	 * used.
	 *
	 * This attribute is used for songs from the database which
	 * have a relative URI.
	 */
	std::string real_uri;

	Tag tag;

	time_t mtime;

	/**
	 * Start of this sub-song within the file.
	 */
	SongTime start_time;

	/**
	 * End of this sub-song within the file.
	 * Unused if zero.
	 */
	SongTime end_time;

	explicit DetachedSong(const LightSong &other);

public:
	explicit DetachedSong(const DetachedSong &) = default;

	explicit DetachedSong(const char *_uri)
		:uri(_uri),
		 mtime(0),
		 start_time(SongTime::zero()), end_time(SongTime::zero()) {}

	explicit DetachedSong(const std::string &_uri)
		:uri(_uri),
		 mtime(0),
		 start_time(SongTime::zero()), end_time(SongTime::zero()) {}

	explicit DetachedSong(std::string &&_uri)
		:uri(std::move(_uri)),
		 mtime(0),
		 start_time(SongTime::zero()), end_time(SongTime::zero()) {}

	template<typename U>
	DetachedSong(U &&_uri, Tag &&_tag)
		:uri(std::forward<U>(_uri)),
		 tag(std::move(_tag)),
		 mtime(0),
		 start_time(SongTime::zero()), end_time(SongTime::zero()) {}

	DetachedSong(DetachedSong &&) = default;

	~DetachedSong();

	gcc_pure
	const char *GetURI() const {
		return uri.c_str();
	}

	template<typename T>
	void SetURI(T &&_uri) {
		uri = std::forward<T>(_uri);
	}

	/**
	 * Does this object have a "real" URI different from the
	 * displayed URI?
	 */
	gcc_pure
	bool HasRealURI() const {
		return !real_uri.empty();
	}

	/**
	 * Returns "real" URI (#real_uri) and falls back to just
	 * GetURI().
	 */
	gcc_pure
	const char *GetRealURI() const {
		return (HasRealURI() ? real_uri : uri).c_str();
	}

	template<typename T>
	void SetRealURI(T &&_uri) {
		real_uri = std::forward<T>(_uri);
	}

	/**
	 * Returns true if both objects refer to the same physical
	 * song.
	 */
	gcc_pure
	bool IsSame(const DetachedSong &other) const {
		return uri == other.uri;
	}

	gcc_pure gcc_nonnull_all
	bool IsURI(const char *other_uri) const {
		return uri == other_uri;
	}

	gcc_pure
	bool IsRemote() const;

	gcc_pure
	bool IsFile() const {
		return !IsRemote();
	}

	gcc_pure
	bool IsAbsoluteFile() const;

	gcc_pure
	bool IsInDatabase() const;

	const Tag &GetTag() const {
		return tag;
	}

	Tag &WritableTag() {
		return tag;
	}

	void SetTag(const Tag &_tag) {
		tag = Tag(_tag);
	}

	void SetTag(Tag &&_tag) {
		tag = std::move(_tag);
	}

	void MoveTagFrom(DetachedSong &&other) {
		tag = std::move(other.tag);
	}

	time_t GetLastModified() const {
		return mtime;
	}

	void SetLastModified(time_t _value) {
		mtime = _value;
	}

	SongTime GetStartTime() const {
		return start_time;
	}

	void SetStartTime(SongTime _value) {
		start_time = _value;
	}

	SongTime GetEndTime() const {
		return end_time;
	}

	void SetEndTime(SongTime _value) {
		end_time = _value;
	}

	gcc_pure
	SignedSongTime GetDuration() const;

	/**
	 * Update the #tag and #mtime.
	 *
	 * @return true on success
	 */
	bool Update();
};

#endif
