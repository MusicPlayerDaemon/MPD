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

#pragma once

#include "protocol/RangeArg.hxx"
#include "tag/Type.h"

struct Queue;
class SongFilter;

/**
 * Describes what part of and how the client wishes to see the queue.
 */
struct QueueSelection {
	/**
	 * An optional pointer to a #SongFilter (not owned by this
	 * object).
	 */
	const SongFilter *filter = nullptr;

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

	[[gnu::pure]]
	bool MatchPosition(const Queue &queue,
			   unsigned position) const noexcept;
};
