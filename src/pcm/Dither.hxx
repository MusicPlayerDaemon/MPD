// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_DITHER_HXX
#define MPD_PCM_DITHER_HXX

#include <cstdint>

enum class SampleFormat : uint8_t;

class PcmDither {
	int32_t error[3];
	int32_t random;

public:
	constexpr PcmDither() noexcept
		:error{0, 0, 0}, random(0) {}

	/**
	 * Shift the given sample by #SBITS-#DBITS to the right, and
	 * apply dithering.
	 *
	 * @tparam ST the input sample type
	 * @tparam SBITS the input bit width
	 * @tparam DBITS the output bit width
	 * @param sample the input sample value
	 */
	template<typename ST, unsigned SBITS, unsigned DBITS>
	ST DitherShift(ST sample) noexcept;

	void Dither24To16(int16_t *dest, const int32_t *src,
			  const int32_t *src_end) noexcept;

	void Dither32To16(int16_t *dest, const int32_t *src,
			  const int32_t *src_end) noexcept;

private:
	/**
	 * Shift the given sample by #scale_bits to the right, and
	 * apply dithering.
	 *
	 * @param T the input sample type
	 * @param MIN the minimum input sample value
	 * @param MAX the maximum input sample value
	 * @param scale_bits the number of bits to be discarded
	 * @param sample the input sample value
	 */
	template<typename T, T MIN, T MAX, unsigned scale_bits>
	T Dither(T sample) noexcept;

	/**
	 * Convert the given sample from one sample format to another,
	 * discarding bits.
	 *
	 * @param ST the input #SampleTraits class
	 * @param ST the output #SampleTraits class
	 * @param sample the input sample value
	 */
	template<typename ST, typename DT>
	typename DT::value_type DitherConvert(typename ST::value_type sample) noexcept;

	template<typename ST, typename DT>
	void DitherConvert(typename DT::pointer dest,
			   typename ST::const_pointer src,
			   typename ST::const_pointer src_end) noexcept;
};

#endif
