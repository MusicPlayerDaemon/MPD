// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_SELECTION_HXX
#define MPD_DATABASE_SELECTION_HXX

#include "protocol/RangeArg.hxx"
#include "tag/Type.hxx"

#include <string>

class SongFilter;
struct LightSong;

struct DatabaseSelection {
	/**
	 * The base URI of the search (UTF-8).  Must not begin or end
	 * with a slash.  An empty string searches the whole database.
	 */
	std::string uri;

	const SongFilter *filter;

	RangeArg window = RangeArg::All();

	/**
	 * Sort the result by the given tag.  #TAG_NUM_OF_ITEM_TYPES
	 * means don't sort.  #SORT_TAG_LAST_MODIFIED sorts by
	 * "Last-Modified" (not technically a tag).
	 */
	TagType sort = TAG_NUM_OF_ITEM_TYPES;

	/**
	 * If #sort is set, this flag can reverse the sort order.
	 */
	bool descending = false;

	/**
	 * Recursively search all sub directories?
	 */
	bool recursive;

	DatabaseSelection(const char *_uri, bool _recursive,
			  const SongFilter *_filter=nullptr) noexcept;

	[[gnu::pure]]
	bool IsFiltered() const noexcept;

	/**
	 * Does this selection contain constraints other than "base"?
	 */
	[[gnu::pure]]
	bool HasOtherThanBase() const noexcept;

	[[gnu::pure]]
	bool Match(const LightSong &song) const noexcept;
};

#endif
