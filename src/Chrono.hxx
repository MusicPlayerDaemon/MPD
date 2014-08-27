/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_CHRONO_HXX
#define MPD_CHRONO_HXX

#include <chrono>
#include <cstdint>

/**
 * A time stamp within a song.  Granularity is 1 millisecond and the
 * maximum value is about 49 days.
 */
class SongTime : public std::chrono::duration<std::uint32_t, std::milli> {
	typedef std::chrono::duration<std::uint32_t, std::milli> Base;
	typedef Base::rep rep;

public:
	SongTime() = default;

	template<typename T>
	explicit constexpr SongTime(T t):Base(t) {}

	static constexpr SongTime FromS(unsigned s) {
		return SongTime(rep(s) * 1000);
	}

	static constexpr SongTime FromS(float s) {
		return SongTime(rep(s * 1000));
	}

	static constexpr SongTime FromS(double s) {
		return SongTime(rep(s * 1000));
	}

	static constexpr SongTime FromMS(rep ms) {
		return SongTime(ms);
	}

	constexpr rep ToMS() const {
		return count();
	}

	template<typename T=rep>
	constexpr T ToScale(unsigned base) const {
		return count() * T(base) / 1000;
	}
};

#endif
