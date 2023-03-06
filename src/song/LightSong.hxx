// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LIGHT_SONG_HXX
#define MPD_LIGHT_SONG_HXX

#include "Chrono.hxx"
#include "pcm/AudioFormat.hxx"

#include <string>
#include <chrono>

struct Tag;

/**
 * A reference to a song file.  Unlike the other "Song" classes in the
 * MPD code base, this one consists only of pointers.  It is supposed
 * to be as light as possible while still providing all the
 * information MPD has about a song file.  This class does not manage
 * any memory, and the pointers become invalid quickly.  Only to be
 * used to pass around during well-defined situations.
 */
struct LightSong {
	/**
	 * If this is not nullptr, then it denotes a prefix for the
	 * #uri.  To build the full URI, join directory and uri with a
	 * slash.
	 */
	const char *directory = nullptr;

	const char *uri;

	/**
	 * The "real" URI, the one to be used for opening the
	 * resource.  If this attribute is nullptr, then #uri (and
	 * #directory) shall be used.
	 *
	 * This attribute is used for songs from the database which
	 * have a relative URI.
	 */
	const char *real_uri = nullptr;

	/**
	 * Metadata.
	 */
	const Tag &tag;

	/**
	 * The time stamp of the last file modification.  A negative
	 * value means that this is unknown/unavailable.
	 */
	std::chrono::system_clock::time_point mtime = std::chrono::system_clock::time_point::min();

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

	/**
	 * Copy of Queue::Item::priority.
	 */
	uint8_t priority = 0;

	LightSong(const char *_uri, const Tag &_tag) noexcept
		:uri(_uri), tag(_tag) {}

	/**
	 * A copy constructor which copies all fields, but only sets
	 * the tag to a caller-provided reference.  This is used by
	 * the #ExportedSong move constructor.
	 */
	LightSong(const LightSong &src, const Tag &_tag) noexcept
		:directory(src.directory), uri(src.uri),
		 real_uri(src.real_uri),
		 tag(_tag),
		 mtime(src.mtime),
		 start_time(src.start_time), end_time(src.end_time),
		 audio_format(src.audio_format) {}

	[[gnu::pure]]
	std::string GetURI() const noexcept {
		if (directory == nullptr)
			return std::string(uri);

		std::string result(directory);
		result.push_back('/');
		result.append(uri);
		return result;
	}

	[[gnu::pure]]
	SignedSongTime GetDuration() const noexcept;
};

#endif
