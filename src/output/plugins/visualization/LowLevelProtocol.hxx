// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef LOW_LEVEL_PROTOCOL_HXX_INCLUDED
#define LOW_LEVEL_PROTOCOL_HXX_INCLUDED

#include "util/PackedBigEndian.hxx"

#include <fftw3.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Visualization {

/*  Write a uint16_t to an output iterator over byte in wire format; return the
 *  iterator in its new position
 */
template <typename OutIter>
OutIter
SerializeU16(uint16_t n, OutIter pout) {
	auto m = PackedBE16(n);
	auto p = (std::byte*)(&m);
	return std::copy(p, p + 2, pout);
}

static_assert(std::numeric_limits<float>::is_iec559);

/* Convert an IEEE 754 single-precision floating-point number to wire format;
 * write it to an output iterator & return the iterator in its new position
 */
template <typename OutIter>
OutIter
SerializeFloat(float f, OutIter pout) {
	auto m = PackedBE32(*(uint32_t*)&f);
	auto p = (std::byte*)(&m);
	return std::copy(p, p + 4, pout);
}

/* Convert an fftwf_complex to wire format; write it to an output iterator &
 * return the iterator in its new position
 */
template <typename OutIter>
OutIter
SerializeComplex(const fftwf_complex c, OutIter pout) {
	auto r = PackedBE32(*(const uint32_t*)&(c[0]));
	auto i = PackedBE32(*(const uint32_t*)&(c[1]));
	auto pr = (std::byte*)(&r);
	auto pi = (std::byte*)(&i);
	pout = std::copy(pr, pr + 4, pout);
	return std::copy(pi, pi + 4, pout);
}

} // namespace Visualization

#endif // LOW_LEVEL_PROTOCOL_HXX_INCLUDED
