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

#ifndef MPD_OUTPUT_SNAPCAST_TIMESTAMP_HXX
#define MPD_OUTPUT_SNAPCAST_TIMESTAMP_HXX

#include "Protocol.hxx"

#include <chrono>

template<typename TimePoint>
constexpr SnapcastTimestamp
ToSnapcastTimestamp(TimePoint t) noexcept
{
	using Clock = typename TimePoint::clock;
	using Duration = typename Clock::duration;

	const auto d = t.time_since_epoch();
	const auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
	const auto rest = d - std::chrono::duration_cast<Duration>(s);
	const auto us = std::chrono::duration_cast<std::chrono::microseconds>(rest);

	SnapcastTimestamp st{};
	st.sec = s.count();
	st.usec = us.count();
	return st;
}

#endif
