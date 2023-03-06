// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Serial.hxx"

#include <atomic>
#include <chrono>

static std::atomic_uint next_serial;

int
GenerateSerial() noexcept
{
	unsigned serial = ++next_serial;
	if (serial < 16) [[unlikely]] {
		/* first-time initialization: seed with a clock value,
		   which is random enough for our use */

		/* this code is not race-free, but good enough */
		using namespace std::chrono;
		const auto now = steady_clock::now().time_since_epoch();
		const auto now_ms = duration_cast<milliseconds>(now);
		const unsigned seed = now_ms.count();
		next_serial = serial = seed;
	}

	return serial;
}

