// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TIMER_HXX
#define MPD_TIMER_HXX

#include <chrono>

struct AudioFormat;

class Timer {
	typedef std::chrono::microseconds Time;

	Time time;
	bool started = false;
	const int rate;
public:
	explicit Timer(AudioFormat af) noexcept;

	bool IsStarted() const noexcept { return started; }

	void Start() noexcept;
	void Reset() noexcept;

	void Add(size_t size) noexcept;

	/**
	 * Returns the duration to sleep to get back to sync.
	 */
	[[gnu::pure]]
	std::chrono::steady_clock::duration GetDelay() const noexcept;

private:
	[[gnu::pure]]
	static Time Now() noexcept {
		return std::chrono::duration_cast<Time>(std::chrono::steady_clock::now().time_since_epoch());
	}
};

#endif
