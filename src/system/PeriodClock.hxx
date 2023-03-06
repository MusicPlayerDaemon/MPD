// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PERIOD_CLOCK_HXX
#define MPD_PERIOD_CLOCK_HXX

#include <chrono>

/**
 * This is a stopwatch which saves the timestamp of an event, and can
 * check whether a specified time span has passed since then.
 */
class PeriodClock {
public:
	typedef std::chrono::steady_clock::duration Duration;
	typedef Duration Delta;
	typedef std::chrono::steady_clock::time_point Stamp;

private:
	Stamp last;

public:
	/**
	 * Initializes the object, setting the last time stamp to "0",
	 * i.e. a Check() will always succeed.  If you do not want this
	 * default behaviour, call Update() immediately after creating the
	 * object.
	 */
	constexpr
	PeriodClock():last() {}

protected:
	static Stamp GetNow() {
		return std::chrono::steady_clock::now();
	}

	constexpr Delta Elapsed(Stamp now) const {
		return last == Stamp()
			? Delta(-1)
			: Delta(now - last);
	}

	constexpr bool Check(Stamp now, Duration duration) const {
		return now >= last + duration;
	}

	void Update(Stamp now) {
		last = now;
	}

public:
	constexpr bool IsDefined() const {
		return last > Stamp();
	}

	/**
	 * Resets the clock.
	 */
	void Reset() {
		last = Stamp();
	}

	/**
	 * Returns the time elapsed since the last update().  Returns
	 * a negative value if update() was never called.
	 */
	Delta Elapsed() const {
		return Elapsed(GetNow());
	}

	/**
	 * Combines a call to Elapsed() and Update().
	 */
	Delta ElapsedUpdate() {
		const auto now = GetNow();
		const auto result = Elapsed(now);
		Update(now);
		return result;
	}

	/**
	 * Checks whether the specified duration has passed since the last
	 * update.
	 *
	 * @param duration the duration
	 */
	bool Check(Duration duration) const {
		return Check(GetNow(), duration);
	}

	/**
	 * Updates the time stamp, setting it to the current clock.
	 */
	void Update() {
		Update(GetNow());
	}

	/**
	 * Updates the time stamp, setting it to the current clock plus the
	 * specified offset.
	 */
	void UpdateWithOffset(Delta offset) {
		Update(GetNow() + offset);
	}

	/**
	 * Checks whether the specified duration has passed since the last
	 * update.  If yes, it updates the time stamp.
	 *
	 * @param duration the duration in milliseconds
	 */
	bool CheckUpdate(Duration duration) {
		Stamp now = GetNow();
		if (Check(now, duration)) {
			Update(now);
			return true;
		} else
			return false;
	}

	/**
	 * Checks whether the specified duration has passed since the last
	 * update.  After that, it updates the time stamp.
	 *
	 * @param duration the duration in milliseconds
	 */
	bool CheckAlwaysUpdate(Duration duration) {
		Stamp now = GetNow();
		bool ret = Check(now, duration);
		Update(now);
		return ret;
	}
};

#endif
