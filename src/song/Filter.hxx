// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_FILTER_HXX
#define MPD_SONG_FILTER_HXX

#include "AndSongFilter.hxx"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

/**
 * Special value for the db_selection_print() sort parameter.
 */
#define SORT_TAG_LAST_MODIFIED (TAG_NUM_OF_ITEM_TYPES + 3)

/**
 * Special value for QueueSelection::sort
 */
#define SORT_TAG_PRIO (TAG_NUM_OF_ITEM_TYPES + 4)

enum TagType : uint8_t;
struct LightSong;

class SongFilter {
	AndSongFilter and_filter;

public:
	SongFilter() = default;

	SongFilter(TagType tag, const char *value, bool fold_case=false);

	~SongFilter();

	SongFilter(SongFilter &&) = default;
	SongFilter &operator=(SongFilter &&) = default;

	/**
	 * Convert this object into an "expression".  This is
	 * only useful for debugging.
	 */
	std::string ToExpression() const noexcept;

private:
	static ISongFilterPtr ParseExpression(const char *&s, bool fold_case=false);

	void Parse(const char *tag, const char *value, bool fold_case=false);

public:
	/**
	 * Throws on error.
	 */
	void Parse(std::span<const char *const> args, bool fold_case=false);

	void Optimize() noexcept;

	[[gnu::pure]]
	bool Match(const LightSong &song) const noexcept;

	const auto &GetItems() const noexcept {
		return and_filter.GetItems();
	}

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return and_filter.IsEmpty();
	}

	/**
	 * Is there at least one item with "fold case" enabled?
	 */
	[[gnu::pure]]
	bool HasFoldCase() const noexcept;

	/**
	 * Does this filter contain constraints other than "base"?
	 */
	[[gnu::pure]]
	bool HasOtherThanBase() const noexcept;

	/**
	 * Returns the "base" specification (if there is one) or
	 * nullptr.
	 */
	[[gnu::pure]]
	const char *GetBase() const noexcept;

	/**
	 * Create a copy of the filter with the given prefix stripped
	 * from all #LOCATE_TAG_BASE_TYPE items.  This is used to
	 * filter songs in mounted databases.
	 */
	SongFilter WithoutBasePrefix(std::string_view prefix) const noexcept;
};

#endif
