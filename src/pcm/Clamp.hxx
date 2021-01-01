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

#ifndef MPD_PCM_UTILS_H
#define MPD_PCM_UTILS_H

#include "util/Compiler.h"

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

	if (gcc_unlikely(x < Traits::MIN))
		return T(Traits::MIN);

	if (gcc_unlikely(x > Traits::MAX))
		return T(Traits::MAX);

	return T(x);
}

#endif
