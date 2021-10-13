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

#ifndef MPD_TAG_BUILDER_HXX
#define MPD_TAG_BUILDER_HXX

#include "Type.h"
#include "Chrono.hxx"

#include <vector>
#include <memory>

struct StringView;
struct TagItem;
struct Tag;

/**
 * A class that constructs #Tag objects.
 */
class TagBuilder {
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

	/** an array of tag items */
	std::vector<TagItem *> items;

public:
	/**
	 * Create an empty tag.
	 */
	TagBuilder() noexcept {
		items.reserve(64);
	}

	~TagBuilder() noexcept {
		Clear();
	}

	TagBuilder(const TagBuilder &other) = delete;

	explicit TagBuilder(const Tag &other) noexcept;
	explicit TagBuilder(Tag &&other) noexcept;

	TagBuilder &operator=(const TagBuilder &other) noexcept;
	TagBuilder &operator=(TagBuilder &&other) noexcept;

	TagBuilder &operator=(Tag &&other) noexcept;

	/**
	 * Returns true if the tag contains no items.  This ignores
	 * the "duration" attribute.
	 */
	bool empty() const {
		return items.empty();
	}

	/**
	 * Returns true if the object contains any information.
	 */
	[[gnu::pure]]
	bool IsDefined() const noexcept {
		return !duration.IsNegative() || has_playlist || !empty();
	}

	void Clear() noexcept;

	/**
	 * Move this object to the given #Tag instance.  This object
	 * is empty afterwards.
	 */
	void Commit(Tag &tag) noexcept;

	/**
	 * Create a new #Tag instance from data in this object.  This
	 * object is empty afterwards.
	 */
	Tag Commit() noexcept;

	/**
	 * Create a new #Tag instance from data in this object.  The
	 * returned object is owned by the caller.  This object is
	 * empty afterwards.
	 */
	std::unique_ptr<Tag> CommitNew() noexcept;

	void SetDuration(SignedSongTime _duration) noexcept {
		duration = _duration;
	}

	void SetHasPlaylist(bool _has_playlist) noexcept {
		has_playlist = _has_playlist;
	}

	void Reserve(unsigned n) noexcept {
		items.reserve(n);
	}

	/**
	 * Checks whether the tag contains one or more items with
	 * the specified type.
	 */
	[[gnu::pure]]
	bool HasType(TagType type) const noexcept;

	/**
	 * Copy attributes and items from the other object that do not
	 * exist in this object.
	 */
	void Complement(const Tag &other) noexcept;

	/**
	 * A variant of AddItem() which does not attempt to fix up the
	 * value and does not check whether the tag type is disabled.
	 */
	void AddItemUnchecked(TagType type, StringView value) noexcept;

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (not null-terminated)
	 * @param length the length of #value
	 */
	void AddItem(TagType type, StringView value) noexcept;

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (null-terminated)
	 */
	[[gnu::nonnull]]
	void AddItem(TagType type, const char *value) noexcept;

	/**
	 * Appends a new tag item with an empty value.  Do not use
	 * this unless you know what you're doing - because usually,
	 * empty values are discarded.
	 */
	void AddEmptyItem(TagType type) noexcept;

	/**
	 * Removes all tag items.
	 */
	void RemoveAll() noexcept;

	/**
	 * Removes all tag items of the specified type.
	 */
	void RemoveType(TagType type) noexcept;

private:
	void AddItemInternal(TagType type, StringView value) noexcept;
};

#endif
