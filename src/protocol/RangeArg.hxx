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

#ifndef MPD_PROTOCOL_RANGE_ARG_HXX
#define MPD_PROTOCOL_RANGE_ARG_HXX

#include <limits>

struct RangeArg {
	unsigned start, end;

	/**
	 * Construct an open-ended range starting at the given index.
	 */
	static constexpr RangeArg OpenEnded(unsigned start) noexcept {
		return { start, std::numeric_limits<unsigned>::max() };
	}

	static constexpr RangeArg All() noexcept {
		return OpenEnded(0);
	}

	/**
	 * Construct an instance describing exactly one index.
	 */
	static constexpr RangeArg Single(unsigned i) noexcept {
		return { i, i + 1 };
	}

	constexpr bool operator==(RangeArg other) const noexcept {
		return start == other.start && end == other.end;
	}

	constexpr bool operator!=(RangeArg other) const noexcept {
		return !(*this == other);
	}

	constexpr bool IsOpenEnded() const noexcept {
		return end == All().end;
	}

	constexpr bool IsAll() const noexcept {
		return *this == All();
	}

	constexpr bool IsWellFormed() const noexcept {
		return start <= end;
	}

	/**
	 * Is this range empty?  A malformed range also counts as
	 * "empty" for this method.
	 */
	constexpr bool IsEmpty() const noexcept {
		return start >= end;
	}

	/**
	 * Check if the range contains at least this number of items.
	 * Unlike Count(), this allows the object to be malformed.
	 */
	constexpr bool HasAtLeast(unsigned n) const noexcept {
		return start + n <= end;
	}

	constexpr bool Contains(unsigned i) const noexcept {
		return i >= start && i < end;
	}

	/**
	 * Count the number of items covered by this range.  This requires the
	 * object to be well-formed.
	 */
	constexpr unsigned Count() const noexcept {
		return end - start;
	}
};

#endif
