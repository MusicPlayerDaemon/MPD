// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <chrono>
#include <utility>
#include <cstdint>

using FloatDuration = std::chrono::duration<double>;

/**
 * A time stamp within a song.  Granularity is 1 millisecond and the
 * maximum value is about 49 days.
 */
class SongTime : public std::chrono::duration<std::uint32_t, std::milli> {
	typedef std::chrono::duration<std::uint32_t, std::milli> Base;

public:
	constexpr SongTime() noexcept = default;

	template<typename T>
	explicit constexpr SongTime(T t) noexcept:Base(t) {}

	static constexpr SongTime zero() noexcept {
		return SongTime(Base::zero());
	}

	template<typename D>
	static constexpr SongTime Cast(D src) noexcept {
		return SongTime(std::chrono::duration_cast<Base>(src));
	}

	static constexpr SongTime FromS(unsigned s) noexcept {
		return SongTime(rep(s) * 1000);
	}

	static constexpr SongTime FromS(float s) noexcept {
		return SongTime(rep(s * 1000));
	}

	static constexpr SongTime FromS(double s) noexcept {
		return SongTime(rep(s * 1000));
	}

	static constexpr SongTime FromMS(rep ms) noexcept {
		return SongTime(ms);
	}

	constexpr rep ToS() const noexcept {
		return count() / rep(1000);
	}

	constexpr rep RoundS() const noexcept {
		return (count() + 500) / rep(1000);
	}

	constexpr rep ToMS() const noexcept {
		return count();
	}

	template<typename T=rep>
	constexpr T ToScale(unsigned scale) const noexcept {
		return count() * T(scale) / 1000;
	}

	/**
	 * Convert a scalar value with the given scale to a #SongTime
	 * instance.
	 *
	 * @param value the input value
	 * @param scale the value's scale in Hz
	 */
	template<typename T=rep>
	static constexpr SongTime FromScale(T value, unsigned scale) noexcept {
		return SongTime(value * T(1000) / T(scale));
	}

	constexpr double ToDoubleS() const noexcept {
		return double(count()) / 1000.;
	}

	constexpr bool IsZero() const noexcept {
		return count() == 0;
	}

	constexpr bool IsPositive() const noexcept {
		return count() > 0;
	}

	constexpr SongTime operator+(const SongTime &other) const noexcept {
		return SongTime(*(const Base *)this + (const Base &)other);
	}

	constexpr SongTime operator-(const SongTime &other) const noexcept {
		return SongTime(*(const Base *)this - (const Base &)other);
	}
};

/**
 * A variant of #SongTime that is based on a signed integer.  It can
 * be used for relative values.
 */
class SignedSongTime : public std::chrono::duration<std::int32_t, std::milli> {
	typedef std::chrono::duration<std::int32_t, std::milli> Base;

public:
	constexpr SignedSongTime() noexcept = default;

	template<typename T>
	explicit constexpr SignedSongTime(T t) noexcept:Base(t) {}

	/**
	 * Allow implicit conversion from SongTime to SignedSongTime.
	 */
	constexpr SignedSongTime(SongTime t) noexcept:Base(t) {}

	static constexpr SignedSongTime zero() noexcept {
		return SignedSongTime(Base::zero());
	}

	/**
	 * Generate a negative value.
	 */
	static constexpr SignedSongTime Negative() noexcept {
		return SignedSongTime(-1);
	}

	template<typename D>
	static constexpr SongTime Cast(D src) noexcept {
		return SongTime(std::chrono::duration_cast<Base>(src));
	}

	static constexpr SignedSongTime FromS(int s) noexcept {
		return SignedSongTime(rep(s) * 1000);
	}

	static constexpr SignedSongTime FromS(unsigned s) noexcept {
		return SignedSongTime(rep(s) * 1000);
	}

	static constexpr SignedSongTime FromS(float s) noexcept {
		return SignedSongTime(rep(s * 1000));
	}

	static constexpr SignedSongTime FromS(double s) noexcept {
		return SignedSongTime(rep(s * 1000));
	}

	static constexpr SignedSongTime FromMS(rep ms) noexcept {
		return SignedSongTime(ms);
	}

	constexpr rep ToS() const noexcept {
		return count() / rep(1000);
	}

	constexpr rep RoundS() const noexcept {
		return (count() + 500) / rep(1000);
	}

	constexpr rep ToMS() const noexcept {
		return count();
	}

	template<typename T=rep>
	constexpr T ToScale(unsigned scale) const noexcept {
		return count() * T(scale) / 1000;
	}

	/**
	 * Convert a scalar value with the given scale to a
	 * #SignedSongTime instance.
	 *
	 * @param value the input value
	 * @param scale the value's scale in Hz
	 */
	template<typename T=rep>
	static constexpr SignedSongTime FromScale(T value, unsigned scale) noexcept {
		return SignedSongTime(value * T(1000) / T(scale));
	}

	constexpr double ToDoubleS() const noexcept {
		return double(count()) / 1000.;
	}

	constexpr bool IsZero() const noexcept {
		return count() == 0;
	}

	constexpr bool IsPositive() const noexcept {
		return count() > 0;
	}

	constexpr bool IsNegative() const noexcept {
		return count() < 0;
	}

	constexpr SignedSongTime operator+(const SignedSongTime &other) const noexcept {
		return SignedSongTime(*(const Base *)this + (const Base &)other);
	}

	constexpr SignedSongTime operator-(const SignedSongTime &other) const noexcept {
		return SignedSongTime(*(const Base *)this - (const Base &)other);
	}
};
