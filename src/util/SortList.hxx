/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StaticVector.hxx"

#include <algorithm> // for std::find_if()
#include <concepts> // for std::predicate

/**
 * Move all items from #src to #dest, keeping both sorted.
 *
 * @param p the predicate by which both lists are already
 * sorted
 */
template<typename List>
constexpr void
MergeList(List &dest, List &src,
#if !defined(ANDROID) && !defined(__APPLE__)
	  /* Android NDK r25b has no std::predicate */
	  std::predicate<typename List::const_reference, typename List::const_reference>
#endif
	  auto p) noexcept
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
#if !defined(ANDROID) && !defined(__APPLE__)
	 /* Android NDK r25b has no std::predicate */
	 std::predicate<typename List::const_reference, typename List::const_reference>
#endif
	 auto p) noexcept
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
