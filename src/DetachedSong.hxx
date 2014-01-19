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
#include "Compiler.h"

#include <string>
#include <utility>

#include <time.h>

struct LightSong;

class DetachedSong {
	friend DetachedSong map_song_detach(const LightSong &song);

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

	Tag tag;

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

	explicit DetachedSong(const LightSong &other);

public:
	explicit DetachedSong(const DetachedSong &other)
		:uri(other.uri),
		 tag(other.tag),
		 mtime(other.mtime),
		 start_ms(other.start_ms), end_ms(other.end_ms) {}

	explicit DetachedSong(const char *_uri)
		:uri(_uri),
		 mtime(0), start_ms(0), end_ms(0) {}

	explicit DetachedSong(const std::string &_uri)
		:uri(_uri),
		 mtime(0), start_ms(0), end_ms(0) {}

	explicit DetachedSong(std::string &&_uri)
		:uri(std::move(_uri)),
		 mtime(0), start_ms(0), end_ms(0) {}

	template<typename U>
	DetachedSong(U &&_uri, Tag &&_tag)
		:uri(std::forward<U>(_uri)),
		 tag(std::move(_tag)),
		 mtime(0), start_ms(0), end_ms(0) {}

	DetachedSong(DetachedSong &&other)
		:uri(std::move(other.uri)),
		 tag(std::move(other.tag)),
		 mtime(other.mtime),
		 start_ms(other.start_ms), end_ms(other.end_ms) {}

	gcc_pure
	const char *GetURI() const {
		return uri.c_str();
	}

	template<typename T>
	void SetURI(T &&_uri) {
		uri = std::forward<T>(_uri);
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
	bool IsInDatabase() const {
		return IsFile() && !IsAbsoluteFile();
	}

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

	unsigned GetStartMS() const {
		return start_ms;
	}

	void SetStartMS(unsigned _value) {
		start_ms = _value;
	}

	unsigned GetEndMS() const {
		return end_ms;
	}

	void SetEndMS(unsigned _value) {
		end_ms = _value;
	}

	gcc_pure
	double GetDuration() const;

	/**
	 * Update the #tag and #mtime.
	 *
	 * @return true on success
	 */
	bool Update();
};

#endif
