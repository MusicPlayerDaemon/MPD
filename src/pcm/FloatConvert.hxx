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

#ifndef MPD_PCM_FLOAT_CONVERT_HXX
#define MPD_PCM_FLOAT_CONVERT_HXX

#include "Traits.hxx"
#include "Clamp.hxx"

/**
 * Convert from float to an integer sample format.
 */
template<SampleFormat F, class Traits=SampleTraits<F>>
struct FloatToIntegerSampleConvert {
	typedef SampleTraits<SampleFormat::FLOAT> SrcTraits;
	typedef Traits DstTraits;

	typedef typename SrcTraits::value_type SV;
	typedef typename SrcTraits::long_type SL;
	typedef typename DstTraits::value_type DV;

	static constexpr SV factor = uintmax_t(1) << (DstTraits::BITS - 1);
	static_assert(factor > 0, "Wrong factor");

	static constexpr DV Convert(SV src) noexcept {
		return PcmClamp<F, Traits>(SL(src * factor));
	}
};

/**
 * Convert from an integer sample format to float.
 */
template<SampleFormat F, class Traits=SampleTraits<F>>
struct IntegerToFloatSampleConvert {
	typedef SampleTraits<SampleFormat::FLOAT> DstTraits;
	typedef Traits SrcTraits;

	typedef typename SrcTraits::value_type SV;
	typedef typename DstTraits::value_type DV;

	static constexpr DV factor = 1.0f / FloatToIntegerSampleConvert<F, Traits>::factor;
	static_assert(factor > 0, "Wrong factor");

	static constexpr DV Convert(SV src) noexcept {
		return DV(src) * factor;
	}
};

#endif
