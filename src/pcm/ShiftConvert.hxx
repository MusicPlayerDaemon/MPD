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

#ifndef MPD_PCM_SHIFT_CONVERT_HXX
#define MPD_PCM_SHIFT_CONVERT_HXX

#include "Traits.hxx"

/**
 * Convert from one integer sample format to another by shifting bits
 * to the left.
 */
template<SampleFormat SF, SampleFormat DF,
	 class ST=SampleTraits<SF>,
	 class DT=SampleTraits<DF>>
struct LeftShiftSampleConvert {
	typedef ST SrcTraits;
	typedef DT DstTraits;

	typedef typename SrcTraits::value_type SV;
	typedef typename DstTraits::value_type DV;

	static_assert(SrcTraits::BITS < DstTraits::BITS,
		      "Source format must be smaller than destination format");

	constexpr static DV Convert(SV src) noexcept {
		return DV(src) << (DstTraits::BITS - SrcTraits::BITS);
	}
};

/**
 * Convert from one integer sample format to another by shifting bits
 * to the right.
 */
template<SampleFormat SF, SampleFormat DF,
	 class ST=SampleTraits<SF>,
	 class DT=SampleTraits<DF>>
struct RightShiftSampleConvert {
	typedef ST SrcTraits;
	typedef DT DstTraits;

	typedef typename SrcTraits::value_type SV;
	typedef typename DstTraits::value_type DV;

	static_assert(SrcTraits::BITS > DstTraits::BITS,
		      "Source format must be smaller than destination format");

	constexpr static DV Convert(SV src) noexcept {
		return src >> (SrcTraits::BITS - DstTraits::BITS);
	}
};

#endif
