// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
