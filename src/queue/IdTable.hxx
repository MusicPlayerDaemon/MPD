// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ID_TABLE_HXX
#define MPD_ID_TABLE_HXX

#include <cassert>

/**
 * A table that maps id numbers to position numbers.
 */
class IdTable {
	const unsigned size;

	/**
	 * How many members of "data" are initialized?
	 *
	 * The initial value is 1 and not 0 because the first element
	 * of the array is never used, because 0 is not a valid song
	 * id.
	 */
	unsigned initialized = 1;

	/**
	 * An incrementing counter helping GenerateId() to generate
	 * the next id.
	 */
	unsigned next = 1;

	/**
	 * A lookup table: the index is the id number and the value is
	 * the position; -1 means this id is unassigned.
	 */
	int *const data = new int[size];

public:
	explicit constexpr IdTable(unsigned _size) noexcept
		:size(_size) {}

	constexpr ~IdTable() noexcept {
		delete[] data;
	}

	IdTable(const IdTable &) = delete;
	IdTable &operator=(const IdTable &) = delete;

	constexpr int IdToPosition(unsigned id) const noexcept {
		return id < initialized
			? data[id]
			: -1;
	}

	constexpr unsigned GenerateId() noexcept {
		assert(next > 0);
		assert(next <= initialized);

		while (true) {
			unsigned id = next;

			++next;
			if (next == size)
				next = 1;

			if (id == initialized) {
				/* the caller will initialize
				   data[id] */
				++initialized;
				return id;
			}

			assert(id < initialized);

			if (data[id] < 0)
				return id;
		}
	}

	constexpr unsigned Insert(unsigned position) noexcept {
		unsigned id = GenerateId();
		assert(id < initialized);
		data[id] = position;
		return id;
	}

	constexpr void Move(unsigned id, unsigned position) noexcept {
		assert(id < initialized);
		assert(data[id] >= 0);

		data[id] = position;
	}

	constexpr void Erase(unsigned id) noexcept {
		assert(id < initialized);
		assert(data[id] >= 0);

		data[id] = -1;
	}
};

#endif
