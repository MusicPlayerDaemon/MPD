// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
