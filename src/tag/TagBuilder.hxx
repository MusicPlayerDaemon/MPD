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

#ifndef MPD_TAG_BUILDER_HXX
#define MPD_TAG_BUILDER_HXX

#include "TagType.h"
#include "Chrono.hxx"
#include "Compiler.h"

#include <vector>

#include <stddef.h>

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
	SignedSongTime duration;

	/**
	 * Does this file have an embedded playlist (e.g. embedded CUE
	 * sheet)?
	 */
	bool has_playlist;

	/** an array of tag items */
	std::vector<TagItem *> items;

public:
	/**
	 * Create an empty tag.
	 */
	TagBuilder()
		:duration(SignedSongTime::Negative()), has_playlist(false) {}

	~TagBuilder() {
		Clear();
	}

	TagBuilder(const TagBuilder &other) = delete;

	explicit TagBuilder(const Tag &other);
	explicit TagBuilder(Tag &&other);

	TagBuilder &operator=(const TagBuilder &other);
	TagBuilder &operator=(TagBuilder &&other);

	TagBuilder &operator=(Tag &&other);

	/**
	 * Returns true if the tag contains no items.  This ignores
	 * the "duration" attribute.
	 */
	bool IsEmpty() const {
		return items.empty();
	}

	/**
	 * Returns true if the object contains any information.
	 */
	gcc_pure
	bool IsDefined() const {
		return !duration.IsNegative() || has_playlist || !IsEmpty();
	}

	void Clear();

	/**
	 * Move this object to the given #Tag instance.  This object
	 * is empty afterwards.
	 */
	void Commit(Tag &tag);

	/**
	 * Create a new #Tag instance from data in this object.  This
	 * object is empty afterwards.
	 */
	Tag Commit();

	/**
	 * Create a new #Tag instance from data in this object.  The
	 * returned object is owned by the caller.  This object is
	 * empty afterwards.
	 */
	Tag *CommitNew();

	void SetDuration(SignedSongTime _duration) {
		duration = _duration;
	}

	void SetHasPlaylist(bool _has_playlist) {
		has_playlist = _has_playlist;
	}

	void Reserve(unsigned n) {
		items.reserve(n);
	}

	/**
	 * Checks whether the tag contains one or more items with
	 * the specified type.
	 */
	gcc_pure
	bool HasType(TagType type) const;

	/**
	 * Copy attributes and items from the other object that do not
	 * exist in this object.
	 */
	void Complement(const Tag &other);

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (not null-terminated)
	 * @param len the length of #value
	 */
	gcc_nonnull_all
	void AddItem(TagType type, const char *value, size_t length);

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (null-terminated)
	 */
	gcc_nonnull_all
	void AddItem(TagType type, const char *value);

	/**
	 * Appends a new tag item with an empty value.  Do not use
	 * this unless you know what you're doing - because usually,
	 * empty values are discarded.
	 */
	void AddEmptyItem(TagType type);

	/**
	 * Removes all tag items.
	 */
	void RemoveAll();

	/**
	 * Removes all tag items of the specified type.
	 */
	void RemoveType(TagType type);

private:
	gcc_nonnull_all
	void AddItemInternal(TagType type, const char *value, size_t length);
};

#endif
