/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_SONG_FILTER_HXX
#define MPD_SONG_FILTER_HXX

#include "AndSongFilter.hxx"
#include "util/Compiler.h"

#include <string>

#include <stdint.h>

/**
 * Special value for the db_selection_print() sort parameter.
 */
#define SORT_TAG_LAST_MODIFIED (TAG_NUM_OF_ITEM_TYPES + 3)

template<typename T> struct ConstBuffer;
enum TagType : uint8_t;
struct LightSong;

class SongFilter {
	AndSongFilter and_filter;

public:
	SongFilter() = default;

	gcc_nonnull(3)
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

	gcc_nonnull(2,3)
	void Parse(const char *tag, const char *value, bool fold_case=false);

public:
	/**
	 * Throws on error.
	 */
	void Parse(ConstBuffer<const char *> args, bool fold_case=false);

	void Optimize() noexcept;

	gcc_pure
	bool Match(const LightSong &song) const noexcept;

	const auto &GetItems() const noexcept {
		return and_filter.GetItems();
	}

	gcc_pure
	bool IsEmpty() const noexcept {
		return and_filter.IsEmpty();
	}

	/**
	 * Is there at least one item with "fold case" enabled?
	 */
	gcc_pure
	bool HasFoldCase() const noexcept;

	/**
	 * Does this filter contain constraints other than "base"?
	 */
	gcc_pure
	bool HasOtherThanBase() const noexcept;

	/**
	 * Returns the "base" specification (if there is one) or
	 * nullptr.
	 */
	gcc_pure
	const char *GetBase() const noexcept;

	/**
	 * Create a copy of the filter with the given prefix stripped
	 * from all #LOCATE_TAG_BASE_TYPE items.  This is used to
	 * filter songs in mounted databases.
	 */
	SongFilter WithoutBasePrefix(const char *prefix) const noexcept;
};

#endif
