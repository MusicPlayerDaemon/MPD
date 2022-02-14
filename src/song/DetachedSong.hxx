/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "tag/Tag.hxx"
#include "pcm/AudioFormat.hxx"
#include "Chrono.hxx"

#include <chrono>
#include <string>
#include <utility>

struct LightSong;
class Storage;
class Path;

/**
 * A stand-alone description of a song, that is, it manages all
 * pointers.  It is called "detached" because it is usually a copy of
 * a #Song (or #LightSong) instance that was detached from the
 * database.
 */
class DetachedSong {
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

	/**
	 * The time stamp of the last file modification.  A negative
	 * value means that this is unknown/unavailable.
	 */
	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();

	/**
	 * Start of this sub-song within the file.
	 */
	SongTime start_time = SongTime::zero();

	/**
	 * End of this sub-song within the file.
	 * Unused if zero.
	 */
	SongTime end_time = SongTime::zero();

	/**
	 * The audio format of the song, if given by the decoder
	 * plugin.  May be undefined if unknown.
	 */
	AudioFormat audio_format = AudioFormat::Undefined();

public:
	explicit DetachedSong(const char *_uri) noexcept
		:uri(_uri) {}

	explicit DetachedSong(const std::string &_uri) noexcept
		:uri(_uri) {}

	explicit DetachedSong(std::string &&_uri) noexcept
		:uri(std::move(_uri)) {}

	template<typename U>
	DetachedSong(U &&_uri, Tag &&_tag) noexcept
		:uri(std::forward<U>(_uri)),
		 tag(std::move(_tag)) {}

	/**
	 * Copy data from a #LightSong instance.  Usually, you should
	 * call DatabaseDetachSong() instead, which initializes
	 * #real_uri properly using Storage::MapUTF8().
	 */
	explicit DetachedSong(const LightSong &other) noexcept;

	~DetachedSong() noexcept = default;

	/* these are declared because the user-defined destructor
	   above prevents them from being generated implicitly */
	explicit DetachedSong(const DetachedSong &) = default;
	DetachedSong(DetachedSong &&) = default;
	DetachedSong &operator=(DetachedSong &&) = default;

	[[gnu::pure]]
	explicit operator LightSong() const noexcept;

	[[gnu::pure]]
	const char *GetURI() const noexcept {
		return uri.c_str();
	}

	template<typename T>
	void SetURI(T &&_uri) noexcept {
		uri = std::forward<T>(_uri);
	}

	/**
	 * Does this object have a "real" URI different from the
	 * displayed URI?
	 */
	[[gnu::pure]]
	bool HasRealURI() const noexcept {
		return !real_uri.empty();
	}

	/**
	 * Returns "real" URI (#real_uri) and falls back to just
	 * GetURI().
	 */
	[[gnu::pure]]
	const char *GetRealURI() const noexcept {
		return (HasRealURI() ? real_uri : uri).c_str();
	}

	template<typename T>
	void SetRealURI(T &&_uri) noexcept {
		real_uri = std::forward<T>(_uri);
	}

	/**
	 * Returns true if both objects refer to the same physical
	 * song.
	 */
	[[gnu::pure]]
	bool IsSame(const DetachedSong &other) const noexcept {
		return uri == other.uri &&
			start_time == other.start_time &&
			end_time == other.end_time;
	}

	[[gnu::pure]] [[gnu::nonnull]]
	bool IsURI(const char *other_uri) const noexcept {
		return uri == other_uri;
	}

	[[gnu::pure]] [[gnu::nonnull]]
	bool IsRealURI(const char *other_uri) const noexcept {
		return (HasRealURI() ? real_uri : uri) == other_uri;
	}

	[[gnu::pure]]
	bool IsRemote() const noexcept;

	[[gnu::pure]]
	bool IsFile() const noexcept {
		return !IsRemote();
	}

	[[gnu::pure]]
	bool IsAbsoluteFile() const noexcept;

	[[gnu::pure]]
	bool IsInDatabase() const noexcept;

	const Tag &GetTag() const noexcept {
		return tag;
	}

	Tag &WritableTag() noexcept {
		return tag;
	}

	void SetTag(const Tag &_tag) noexcept {
		tag = Tag(_tag);
	}

	void SetTag(Tag &&_tag) noexcept {
		tag = std::move(_tag);
	}

	void MoveTagFrom(DetachedSong &&other) noexcept {
		tag = std::move(other.tag);
	}

	/**
	 * Similar to the MoveTagFrom(), but move only the #TagItem
	 * array.
	 */
	void MoveTagItemsFrom(DetachedSong &&other) noexcept {
		tag.MoveItemsFrom(std::move(other.tag));
	}

	std::chrono::system_clock::time_point GetLastModified() const noexcept {
		return mtime;
	}

	void SetLastModified(std::chrono::system_clock::time_point _value) noexcept {
		mtime = _value;
	}

	SongTime GetStartTime() const noexcept {
		return start_time;
	}

	void SetStartTime(SongTime _value) noexcept {
		start_time = _value;
	}

	SongTime GetEndTime() const noexcept {
		return end_time;
	}

	void SetEndTime(SongTime _value) noexcept {
		end_time = _value;
	}

	[[gnu::pure]]
	SignedSongTime GetDuration() const noexcept;

	const AudioFormat &GetAudioFormat() const noexcept {
		return audio_format;
	}

	void SetAudioFormat(const AudioFormat &src) noexcept {
		audio_format = src;
	}

	/**
	 * Update the #tag and #mtime.
	 *
	 * Throws on error.
	 *
	 * @return true on success
	 */
	bool Update();

	/**
	 * Load #tag and #mtime from a local file.
	 *
	 * Throws on error.
	 */
	bool LoadFile(Path path);
};

#endif
