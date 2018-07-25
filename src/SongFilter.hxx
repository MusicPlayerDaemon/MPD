/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "lib/icu/Compare.hxx"
#include "Compiler.h"

#include <memory>
#include <string>
#include <list>
#include <chrono>

#include <stdint.h>

/**
 * Special value for the db_selection_print() sort parameter.
 */
#define SORT_TAG_LAST_MODIFIED (TAG_NUM_OF_ITEM_TYPES + 3)

template<typename T> struct ConstBuffer;
enum TagType : uint8_t;
struct Tag;
struct TagItem;
struct LightSong;
class ISongFilter;
using ISongFilterPtr = std::unique_ptr<ISongFilter>;

class ISongFilter {
public:
	virtual ~ISongFilter() noexcept {}

	virtual ISongFilterPtr Clone() const noexcept = 0;

	/**
	 * Convert this object into an "expression".  This is
	 * only useful for debugging.
	 */
	virtual std::string ToExpression() const noexcept = 0;

	gcc_pure
	virtual bool Match(const LightSong &song) const noexcept = 0;
};

class StringFilter {
	std::string value;

	/**
	 * This value is only set if case folding is enabled.
	 */
	IcuCompare fold_case;

public:
	template<typename V>
	StringFilter(V &&_value, bool _fold_case)
		:value(std::forward<V>(_value)),
		 fold_case(_fold_case
			   ? IcuCompare(value.c_str())
			   : IcuCompare()) {}

	bool empty() const noexcept {
		return value.empty();
	}

	const auto &GetValue() const noexcept {
		return value;
	}

	bool GetFoldCase() const noexcept {
		return fold_case;
	}

	gcc_pure
	bool Match(const char *s) const noexcept;
};

class UriSongFilter final : public ISongFilter {
	StringFilter filter;

	bool negated;

public:
	template<typename V>
	UriSongFilter(V &&_value, bool fold_case, bool _negated)
		:filter(std::forward<V>(_value), fold_case),
		 negated(_negated) {}

	const auto &GetValue() const noexcept {
		return filter.GetValue();
	}

	bool GetFoldCase() const {
		return filter.GetFoldCase();
	}

	bool IsNegated() const noexcept {
		return negated;
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<UriSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

class BaseSongFilter final : public ISongFilter {
	std::string value;

public:
	BaseSongFilter(const BaseSongFilter &) = default;

	template<typename V>
	explicit BaseSongFilter(V &&_value)
		:value(std::forward<V>(_value)) {}

	const char *GetValue() const noexcept {
		return value.c_str();
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<BaseSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

class TagSongFilter final : public ISongFilter {
	TagType type;

	bool negated;

	StringFilter filter;

public:
	template<typename V>
	TagSongFilter(TagType _type, V &&_value, bool fold_case, bool _negated)
		:type(_type), negated(_negated),
		 filter(std::forward<V>(_value), fold_case) {}

	TagType GetTagType() const {
		return type;
	}

	const auto &GetValue() const noexcept {
		return filter.GetValue();
	}

	bool GetFoldCase() const {
		return filter.GetFoldCase();
	}

	bool IsNegated() const noexcept {
		return negated;
	}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<TagSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;

private:
	bool MatchNN(const Tag &tag) const noexcept;
	bool MatchNN(const TagItem &tag) const noexcept;
};

class ModifiedSinceSongFilter final : public ISongFilter {
	std::chrono::system_clock::time_point value;

public:
	explicit ModifiedSinceSongFilter(std::chrono::system_clock::time_point _value) noexcept
		:value(_value) {}

	ISongFilterPtr Clone() const noexcept override {
		return std::make_unique<ModifiedSinceSongFilter>(*this);
	}

	std::string ToExpression() const noexcept override;
	bool Match(const LightSong &song) const noexcept override;
};

class SongFilter {
	std::list<ISongFilterPtr> items;

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
	ISongFilterPtr ParseExpression(const char *&s, bool fold_case=false);

	gcc_nonnull(2,3)
	void Parse(const char *tag, const char *value, bool fold_case=false);

public:
	/**
	 * Throws on error.
	 */
	void Parse(ConstBuffer<const char *> args, bool fold_case=false);

	gcc_pure
	bool Match(const LightSong &song) const noexcept;

	const auto &GetItems() const noexcept {
		return items;
	}

	gcc_pure
	bool IsEmpty() const noexcept {
		return items.empty();
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
