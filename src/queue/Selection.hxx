// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "protocol/RangeArg.hxx"
#include "tag/Type.hxx"

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
