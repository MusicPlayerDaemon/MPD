// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "HugeAllocator.hxx"

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

	void SetName(const char *name) noexcept {
		buffer.SetName(name);
	}

	struct ReadResult {
		size_type undefined_size;
		std::span<const T> defined_buffer;

		constexpr bool HasData() const noexcept {
			return undefined_size == 0 &&
				!defined_buffer.empty();
		}
	};

	ReadResult Read(size_type offset) const noexcept {
		auto c = map.Check(offset);
		return {c.undefined_size, {&buffer.front() + offset + c.undefined_size, c.defined_size}};
	}

	std::span<T> Write(size_type offset) noexcept {
		auto c = map.Check(offset);
		return {&buffer.front() + offset, c.undefined_size};
	}

	void Commit(size_type start_offset, size_type end_offset) noexcept {
		map.Commit(start_offset, end_offset);
	}
};
