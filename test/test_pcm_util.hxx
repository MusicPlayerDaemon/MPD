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

#include "util/ConstBuffer.hxx"

#include <gtest/gtest.h>

#include <array>
#include <random>

#include <stddef.h>
#include <stdint.h>

template<typename T>
struct RandomInt {
	typedef std::minstd_rand Engine;
	Engine engine;

	static_assert(sizeof(T) <= sizeof(Engine::result_type),
		      "RNG result type too small");

	T operator()() {
		return T(engine());
	}
};

struct RandomInt24 : RandomInt<int32_t> {
	int32_t operator()() {
		auto t = RandomInt::operator()();
		t &= 0xffffff;
		if (t & 0x800000)
			t |= 0xff000000;
		return t;
	}
};

struct RandomFloat {
	std::mt19937 gen;
	std::uniform_real_distribution<float> dis;

	RandomFloat():gen(std::random_device()()), dis(-1.0, 1.0) {}

	float operator()() {
		return dis(gen);
	}
};

template<typename T, size_t N>
class TestDataBuffer : std::array<T, N> {
public:
	using typename std::array<T, N>::const_pointer;
	using std::array<T, N>::size;
	using std::array<T, N>::begin;
	using std::array<T, N>::end;
	using std::array<T, N>::operator[];

	template<typename G=RandomInt<T>>
	TestDataBuffer(G g=G()):std::array<T, N>() {
		for (auto &i : *this)
			i = g();

	}

	operator typename std::array<T, N>::const_pointer() const {
		return begin();
	}

	operator ConstBuffer<T>() const {
		return { begin(), size() };
	}

	operator ConstBuffer<void>() const {
		return { begin(), size() * sizeof(T) };
	}
};

template<typename T>
bool
AssertEqualWithTolerance(const T &a, const T &b, unsigned tolerance)
{
	EXPECT_EQ(a.size(), b.size());

	for (unsigned i = 0; i < a.size(); ++i) {
		int64_t x = a[i], y = b[i];

		EXPECT_GE(x, y - int64_t(tolerance));
		EXPECT_LE(x, y + int64_t(tolerance));
	}

	return true;
}
