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

#ifndef MPD_TAG_HXX
#define MPD_TAG_HXX

#include "Type.h" // IWYU pragma: export
#include "Item.hxx" // IWYU pragma: export
#include "Chrono.hxx"
#include "util/DereferenceIterator.hxx"

#include <memory>
#include <utility>

/**
 * The meta information about a song file.  It is a MPD specific
 * subset of tags (e.g. from ID3, vorbis comments, ...).
 */
struct Tag {
	/**
	 * The duration of the song.  A negative value means that the
	 * length is unknown.
	 */
	SignedSongTime duration = SignedSongTime::Negative();

	/**
	 * Does this file have an embedded playlist (e.g. embedded CUE
	 * sheet)?
	 */
	bool has_playlist = false;

	/** the total number of tag items in the #items array */
	unsigned short num_items = 0;

	/** an array of tag items */
	TagItem **items = nullptr;

	/**
	 * Create an empty tag.
	 */
	Tag() = default;

	Tag(const Tag &other) noexcept;

	Tag(Tag &&other) noexcept
		:duration(other.duration), has_playlist(other.has_playlist),
		 num_items(other.num_items), items(other.items) {
		other.items = nullptr;
		other.num_items = 0;
	}

	/**
	 * Free the tag object and all its items.
	 */
	~Tag() noexcept {
		Clear();
	}

	Tag &operator=(const Tag &other) = delete;

	Tag &operator=(Tag &&other) noexcept {
		duration = other.duration;
		has_playlist = other.has_playlist;
		MoveItemsFrom(std::move(other));
		return *this;
	}

	/**
	 * Similar to the move operator, but move only the #TagItem
	 * array.
	 */
	void MoveItemsFrom(Tag &&other) noexcept {
		std::swap(items, other.items);
		std::swap(num_items, other.num_items);
	}

	/**
	 * Returns true if the tag contains no items.  This ignores
	 * the "duration" attribute.
	 */
	bool IsEmpty() const noexcept {
		return num_items == 0;
	}

	/**
	 * Returns true if the tag contains any information.
	 */
	bool IsDefined() const noexcept {
		return !IsEmpty() || !duration.IsNegative();
	}

	/**
	 * Clear everything, as if this was a new Tag object.
	 */
	void Clear() noexcept;

	/**
	 * Merges the data from two tags.  If both tags share data for the
	 * same TagType, only data from "add" is used.
	 *
	 * @return a newly allocated tag
	 */
	static Tag Merge(const Tag &base,
			 const Tag &add) noexcept;

	static std::unique_ptr<Tag> MergePtr(const Tag &base,
					     const Tag &add) noexcept;

	/**
	 * Merges the data from two tags.  Any of the two may be nullptr.  Both
	 * are freed by this function.
	 *
	 * @return a newly allocated tag
	 */
	static std::unique_ptr<Tag> Merge(std::unique_ptr<Tag> base,
					  std::unique_ptr<Tag> add) noexcept;

	/**
	 * Merges the data from two tags.  Any of the two may be nullptr.
	 *
	 * @return a newly allocated tag (or nullptr if both
	 * parameters are nullptr)
	 */
	static std::unique_ptr<Tag> Merge(const Tag *base,
					  const Tag *add) noexcept;

	/**
	 * Returns the first value of the specified tag type, or
	 * nullptr if none is present in this tag object.
	 */
	[[gnu::pure]]
	const char *GetValue(TagType type) const noexcept;

	/**
	 * Checks whether the tag contains one or more items with
	 * the specified type.
	 */
	[[gnu::pure]]
	bool HasType(TagType type) const noexcept;

	/**
	 * Returns a value for sorting on the specified type, with
	 * automatic fallbacks to the next best tag type
	 * (e.g. #TAG_ALBUM_ARTIST falls back to #TAG_ARTIST).  If
	 * there is no such value, returns an empty string.
	 */
	[[gnu::pure]] [[gnu::returns_nonnull]]
	const char *GetSortValue(TagType type) const noexcept;

	using const_iterator = DereferenceIterator<TagItem *const*,
						   const TagItem>;

	const_iterator begin() const noexcept {
		return const_iterator{items};
	}

	const_iterator end() const noexcept {
		return const_iterator{items + num_items};
	}
};

#endif
