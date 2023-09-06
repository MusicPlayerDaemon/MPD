// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_BUILDER_HXX
#define MPD_TAG_BUILDER_HXX

#include "Type.hxx"
#include "Chrono.hxx"

#include <memory>
#include <string_view>
#include <vector>

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
	void AddItemUnchecked(TagType type, std::string_view value) noexcept;

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (not null-terminated)
	 * @param length the length of #value
	 */
	void AddItem(TagType type, std::string_view value) noexcept;

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
	void AddItemInternal(TagType type, std::string_view value) noexcept;
};

#endif
