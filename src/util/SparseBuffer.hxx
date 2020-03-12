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

#ifndef SPARSE_BUFFER_HXX
#define SPARSE_BUFFER_HXX

#include "HugeAllocator.hxx"
#include "ConstBuffer.hxx"
#include "WritableBuffer.hxx"

#include <cassert>
#include <map>

/**
 * Helper class for #SparseBuffer which describes which portions of
 * the buffer have "known" data.
 */
class SparseMap {
	using size_type = std::size_t;

	/**
	 * Key is start offset, value is end offset.
	 */
	using Map = std::map<size_type, size_type>;
	using Iterator = typename Map::iterator;

	Map map;

public:
	explicit SparseMap(size_type size) noexcept
		:map{{size, size}} {
		assert(size > 0);
	}

	size_type size() const noexcept {
		return GetEndOffset();
	}

	struct CheckResult {
		size_type undefined_size;
		size_type defined_size;
	};

	/**
	 * Check and classify the given offset.  Returns a structure
	 * which tells you how much data is undefined, and how much
	 * data follows which is defined.
	 */
	CheckResult Check(size_type offset) const noexcept;

	/**
	 * Commit a write: mark the given range in the buffer as
	 * "defined".
	 */
	void Commit(size_type start_offset, size_type end_offset) noexcept;

private:
	size_type GetEndOffset() const noexcept {
		return std::prev(map.end())->second;
	}

	Iterator CheckCollapsePrevious(Iterator i) noexcept;
	Iterator CheckCollapseNext(Iterator i) noexcept;
};

/**
 * A buffer which caches the contents of a "huge" array, and remembers
 * which chunks are available.
 */
template<typename T>
class SparseBuffer {
	using Buffer = HugeArray<T>;
	using size_type = typename Buffer::size_type;

	Buffer buffer;

	SparseMap map;

public:
	explicit SparseBuffer(size_type size)
		:buffer(size), map(size) {
		buffer.ForkCow(false);
	}

	size_type size() const noexcept {
		return map.size();
	}

	struct ReadResult {
		size_type undefined_size;
		ConstBuffer<T> defined_buffer;

		constexpr bool HasData() const noexcept {
			return undefined_size == 0 &&
				!defined_buffer.empty();
		}
	};

	ReadResult Read(size_type offset) const noexcept {
		auto c = map.Check(offset);
		return {c.undefined_size, {&buffer.front() + offset + c.undefined_size, c.defined_size}};
	}

	WritableBuffer<T> Write(size_type offset) noexcept {
		auto c = map.Check(offset);
		return {&buffer.front() + offset, c.undefined_size};
	}

	void Commit(size_type start_offset, size_type end_offset) noexcept {
		map.Commit(start_offset, end_offset);
	}
};

#endif
