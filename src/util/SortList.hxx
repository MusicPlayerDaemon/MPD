// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Concepts.hxx"
#include "StaticVector.hxx"

#include <algorithm> // for std::find_if()

/**
 * Move all items from #src to #dest, keeping both sorted.
 *
 * @param p the predicate by which both lists are already
 * sorted
 */
template<typename List>
constexpr void
MergeList(List &dest, List &src,
	  Predicate<typename List::const_reference, typename List::const_reference> auto p) noexcept
{
	const auto dest_end = dest.end(), src_end = src.end();

	auto dest_at = dest.begin();

	while (!src.empty()) {
		const auto src_begin = src.begin();

		/* find the first item of "dest" that is larger than
		   the front of "src"; this is the next insertion
		   position */
		dest_at = std::find_if(dest_at, dest_end, [&p, &src_front = *src_begin](const auto &i){
			return p(src_front, i);
		});

		if (dest_at == dest_end) {
			/* all items in "src" are larger than
			   "this": splice the whole list at
			   the end of "this" */
			dest.splice(dest_end, src);
			break;
		}

		/* find the first item of "src" that is not smaller
		   than the "dest" insertion anchor; this is the end
		   of the range of items to be spliced */
		const auto &dest_anchor = *dest_at;
		typename List::size_type n = 1;
		auto src_until = std::next(src_begin);
		while (src_until != src_end && p(*src_until, dest_anchor)) {
			++src_until;
			++n;
		}

		dest.splice(dest_at, src, src_begin, src_until, n);
	}
}

template<typename List>
constexpr void
SortList(List &list,
	 Predicate <typename List::const_reference, typename List::const_reference> auto p) noexcept
{
	using std::swap;

	if (list.empty())
		return;

	/* bottom-up merge sort */

	List carry;
	StaticVector<List, 64> array;

	while (!list.empty()) {
		carry.splice(carry.begin(), list, list.begin());

		std::size_t i = 0;
		while (i < array.size() && !array[i].empty()) {
			auto &c = array[i++];
			MergeList(c, carry, p);
			swap(carry, c);
		}

		if (i == array.size())
			array.emplace_back();
		swap(carry, array[i]);
	}

	assert(!array.empty());

	for (std::size_t i = 1; i < array.size(); ++i)
		MergeList(array[i], array[i - 1], p);

	swap(list, array.back());
}
