/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_TAG_HANDLER_HXX
#define MPD_TAG_HANDLER_HXX

#include "check.h"
#include "Type.h"
#include "Chrono.hxx"
#include "Compiler.h"

class TagBuilder;

/**
 * An interface for receiving metadata of a song.
 */
class TagHandler {
	const unsigned want_mask;

public:
	static constexpr unsigned WANT_DURATION = 0x1;
	static constexpr unsigned WANT_TAG = 0x2;
	static constexpr unsigned WANT_PAIR = 0x4;

	explicit TagHandler(unsigned _want_mask) noexcept
		:want_mask(_want_mask) {}

	TagHandler(const TagHandler &) = delete;
	TagHandler &operator=(const TagHandler &) = delete;

	bool WantDuration() const noexcept {
		return want_mask & WANT_DURATION;
	}

	bool WantTag() const noexcept {
		return want_mask & WANT_TAG;
	}

	bool WantPair() const noexcept {
		return want_mask & WANT_PAIR;
	}

	/**
	 * Declare the duration of a song.  Do not call
	 * this when the duration could not be determined, because
	 * there is no magic value for "unknown duration".
	 */
	virtual void OnDuration(SongTime duration) noexcept = 0;

	/**
	 * A tag has been read.
	 *
	 * @param the value of the tag; the pointer will become
	 * invalid after returning
	 */
	virtual void OnTag(TagType type, const char *value) noexcept = 0;

	/**
	 * A name-value pair has been read.  It is the codec specific
	 * representation of tags.
	 */
	virtual void OnPair(const char *key, const char *value) noexcept = 0;
};

class NullTagHandler : public TagHandler {
public:
	explicit NullTagHandler(unsigned _want_mask) noexcept
		:TagHandler(_want_mask) {}

	void OnDuration(gcc_unused SongTime duration) noexcept override {}
	void OnTag(gcc_unused TagType type,
		   gcc_unused const char *value) noexcept override {}
	void OnPair(gcc_unused const char *key,
		    gcc_unused const char *value) noexcept override {}
};

/**
 * This #TagHandler implementation adds tag values to a #TagBuilder
 * object.
 */
class AddTagHandler : public NullTagHandler {
protected:
	TagBuilder &tag;

	AddTagHandler(unsigned _want_mask, TagBuilder &_builder) noexcept
		:NullTagHandler(_want_mask), tag(_builder) {}

public:
	explicit AddTagHandler(TagBuilder &_builder) noexcept
		:AddTagHandler(WANT_DURATION|WANT_TAG, _builder) {}

	void OnDuration(SongTime duration) noexcept override;
	void OnTag(TagType type, const char *value) noexcept override;
};

/**
 * This #TagHandler implementation adds tag values to a #TagBuilder object
 * (casted from the context pointer), and supports the has_playlist
 * attribute.
 */
class FullTagHandler : public AddTagHandler {
public:
	explicit FullTagHandler(TagBuilder &_builder) noexcept
		:AddTagHandler(WANT_DURATION|WANT_TAG|WANT_PAIR, _builder) {}

	void OnPair(const char *key, const char *value) noexcept override;
};

#endif
