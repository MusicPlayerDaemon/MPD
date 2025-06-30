// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <chrono>

/**
 * Cache the now() method of a clock.
 */
template<typename Clock>
class ClockCache {
	using value_type = typename Clock::time_point;
	mutable value_type value;

public:
	ClockCache() = default;
	ClockCache(const ClockCache &) = delete;
	ClockCache &operator=(const ClockCache &) = delete;

	[[gnu::pure]]
	const auto &now() const noexcept {
		if (value <= value_type())
			value = Clock::now();
		return value;
	}

	void flush() noexcept {
		value = {};
	}

	/**
	 * Inject a fake value.  This can be helpful for unit tests.
	 */
	void Mock(value_type _value) noexcept {
		value = _value;
	}
};
