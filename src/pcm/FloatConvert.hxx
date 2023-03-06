// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
