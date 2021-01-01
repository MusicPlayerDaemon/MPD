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

#ifndef MPD_CHRONO_HXX
#define MPD_CHRONO_HXX

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
	SongTime() = default;

	template<typename T>
	explicit constexpr SongTime(T t):Base(t) {}

	static constexpr SongTime zero() {
		return SongTime(Base::zero());
	}

	template<typename D>
	static constexpr SongTime Cast(D src) {
		return SongTime(std::chrono::duration_cast<Base>(src));
	}

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

	constexpr rep ToS() const {
		return count() / rep(1000);
	}

	constexpr rep RoundS() const {
		return (count() + 500) / rep(1000);
	}

	constexpr rep ToMS() const {
		return count();
	}

	template<typename T=rep>
	constexpr T ToScale(unsigned scale) const {
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
	static constexpr SongTime FromScale(T value, unsigned scale) {
		return SongTime(value * T(1000) / T(scale));
	}

	constexpr double ToDoubleS() const {
		return double(count()) / 1000.;
	}

	constexpr bool IsZero() const {
		return count() == 0;
	}

	constexpr bool IsPositive() const {
		return count() > 0;
	}

	constexpr SongTime operator+(const SongTime &other) const {
		return SongTime(*(const Base *)this + (const Base &)other);
	}

	constexpr SongTime operator-(const SongTime &other) const {
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
	SignedSongTime() = default;

	template<typename T>
	explicit constexpr SignedSongTime(T t):Base(t) {}

	/**
	 * Allow implicit conversion from SongTime to SignedSongTime.
	 */
	constexpr SignedSongTime(SongTime t):Base(t) {}

	static constexpr SignedSongTime zero() {
		return SignedSongTime(Base::zero());
	}

	/**
	 * Generate a negative value.
	 */
	static constexpr SignedSongTime Negative() {
		return SignedSongTime(-1);
	}

	template<typename D>
	static constexpr SongTime Cast(D src) {
		return SongTime(std::chrono::duration_cast<Base>(src));
	}

	static constexpr SignedSongTime FromS(int s) {
		return SignedSongTime(rep(s) * 1000);
	}

	static constexpr SignedSongTime FromS(unsigned s) {
		return SignedSongTime(rep(s) * 1000);
	}

	static constexpr SignedSongTime FromS(float s) {
		return SignedSongTime(rep(s * 1000));
	}

	static constexpr SignedSongTime FromS(double s) {
		return SignedSongTime(rep(s * 1000));
	}

	static constexpr SignedSongTime FromMS(rep ms) {
		return SignedSongTime(ms);
	}

	constexpr rep ToS() const {
		return count() / rep(1000);
	}

	constexpr rep RoundS() const {
		return (count() + 500) / rep(1000);
	}

	constexpr rep ToMS() const {
		return count();
	}

	template<typename T=rep>
	constexpr T ToScale(unsigned scale) const {
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
	static constexpr SignedSongTime FromScale(T value, unsigned scale) {
		return SignedSongTime(value * T(1000) / T(scale));
	}

	constexpr double ToDoubleS() const {
		return double(count()) / 1000.;
	}

	constexpr bool IsZero() const {
		return count() == 0;
	}

	constexpr bool IsPositive() const {
		return count() > 0;
	}

	constexpr bool IsNegative() const {
		return count() < 0;
	}

	constexpr SignedSongTime operator+(const SignedSongTime &other) const {
		return SignedSongTime(*(const Base *)this + (const Base &)other);
	}

	constexpr SignedSongTime operator-(const SignedSongTime &other) const {
		return SignedSongTime(*(const Base *)this - (const Base &)other);
	}
};

#endif
