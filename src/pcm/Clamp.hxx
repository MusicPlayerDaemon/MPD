// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_UTILS_H
#define MPD_PCM_UTILS_H

#include <cstdint>
#include <limits>

enum class SampleFormat : uint8_t;
template<SampleFormat F> struct SampleTraits;

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
template<SampleFormat F, class Traits=SampleTraits<F>>
constexpr typename Traits::value_type
PcmClamp(typename Traits::long_type x) noexcept
{
	typedef typename Traits::value_type T;

	typedef std::numeric_limits<T> limits;
	static_assert(Traits::MIN >= limits::min(), "out of range");
	static_assert(Traits::MAX <= limits::max(), "out of range");

	if (x < Traits::MIN) [[unlikely]]
		return T(Traits::MIN);

	if (x > Traits::MAX) [[unlikely]]
		return T(Traits::MAX);

	return T(x);
}

#endif
