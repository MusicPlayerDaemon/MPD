/*
 * Copyright (C) 2013-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "SparseBuffer.hxx"

SparseMap::CheckResult
SparseMap::Check(size_type offset) const noexcept
{
	assert(!map.empty());
	assert(offset < GetEndOffset());

	auto i = map.lower_bound(offset);
	if (i == map.end())
		/* from here up until the end of the file is
		   defined */
		return {0, std::prev(i)->second - offset};

	assert(i->first >= offset);

	if (offset == i->first)
		/* at the beginning of this chunk: the whole chunk can
		   be read */
		return {0, i->second - offset};

	if (i == map.begin())
		/* before the very first chunk: there's a hole, and
		   after the hole, the whole chunk can be read */
		return {i->first - offset, i->second - i->first};

	auto p = std::prev(i);
	assert(p->first < offset);

	if (offset >= p->second)
		/* between two chunks */
		return {i->first - offset, i->second - i->first};

	/* inside a chunk: the rest of the chunk can be read */
	return {0, p->second - offset};
}

void
SparseMap::Commit(size_type start_offset, size_type end_offset) noexcept
{
	assert(start_offset < end_offset);

	auto e = map.emplace(start_offset, end_offset);
	if (!e.second && end_offset > e.first->second)
		e.first->second = end_offset;

	CheckCollapseNext(CheckCollapsePrevious(e.first));
}

inline SparseMap::Iterator
SparseMap::CheckCollapsePrevious(Iterator i) noexcept
{
	assert(i != map.end());

	while (i != map.begin()) {
		auto previous = std::prev(i);
		if (previous->second < i->first)
			break;

		if (i->second > previous->second)
			previous->second = i->second;

		map.erase(i);
		i = previous;
	}

	return i;
}

inline SparseMap::Iterator
SparseMap::CheckCollapseNext(Iterator i) noexcept
{
	assert(i != map.end());

	for (auto next = std::next(i);
	     next != map.end() && i->second >= next->first;) {
		if (next->second > i->second)
			i->second = next->second;

		next = map.erase(next);
	}

	return i;
}
