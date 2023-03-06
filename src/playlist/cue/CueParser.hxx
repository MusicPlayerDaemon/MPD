// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CUE_PARSER_HXX
#define MPD_CUE_PARSER_HXX

#include "song/DetachedSong.hxx"
#include "tag/Builder.hxx"

#include <memory>
#include <string>
#include <string_view>

class CueParser {
	enum {
		/**
		 * Parsing the CUE header.
		 */
		HEADER,

		/**
		 * Parsing a "FILE ... WAVE".
		 */
		WAVE,

		/**
		 * Ignore everything until the next "FILE".
		 */
		IGNORE_FILE,

		/**
		 * Parsing a "TRACK ... AUDIO".
		 */
		TRACK,

		/**
		 * Ignore everything until the next "TRACK".
		 */
		IGNORE_TRACK,
	} state = HEADER;

	/**
	 * Tags read from the CUE header.
	 */
	TagBuilder header_tag;

	/**
	 * Tags read for the current song (attribute #current).  When
	 * #current gets moved to #previous, TagBuilder::Commit() will
	 * be called.
	 */
	TagBuilder song_tag;

	std::string filename;

	/**
	 * The song currently being edited.
	 */
	std::unique_ptr<DetachedSong> current;

	/**
	 * The previous song.  It is remembered because its end_time
	 * will be set to the current song's start time.
	 */
	std::unique_ptr<DetachedSong> previous;

	/**
	 * A song that is completely finished and can be returned to
	 * the caller via Get().
	 */
	std::unique_ptr<DetachedSong> finished;

	/**
	 * Ignore "INDEX" lines?  Only up the first one after "00" is
	 * used.  If there is a pregap (INDEX 00..01), it is assigned
	 * to the previous song.
	 */
	bool ignore_index;

	/**
	 * Tracks whether Finish() has been called.  If true, then all
	 * remaining (partial) results will be delivered by Get().
	 */
	bool end = false;

public:
	/**
	 * Feed a text line from the CUE file into the parser.  Call
	 * Get() after this to see if a song has been finished.
	 */
	void Feed(std::string_view line) noexcept;

	/**
	 * Tell the parser that the end of the file has been reached.  Call
	 * Get() after this to see if a song has been finished.
	 * This procedure must be done twice!
	 */
	void Finish() noexcept;

	/**
	 * Check if a song was finished by the last Feed() or Finish()
	 * call.
	 *
	 * @return a song object that must be freed by the caller, or NULL if
	 * no song was finished at this time
	 */
	std::unique_ptr<DetachedSong> Get() noexcept;

private:
	[[gnu::pure]]
	TagBuilder *GetCurrentTag() noexcept;

	/**
	 * Commit the current song.  It will be moved to "previous",
	 * so the next song may soon edit its end time (using the next
	 * song's start time).
	 */
	void Commit() noexcept;
};

#endif
