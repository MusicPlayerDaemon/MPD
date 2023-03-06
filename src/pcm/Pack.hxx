// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * Library for working with packed 24 bit samples.
 */

#ifndef PCM_PACK_HXX
#define PCM_PACK_HXX

#include <cstdint>

/**
 * Converts padded 24 bit samples (4 bytes per sample) to packed 24
 * bit samples (3 bytes per sample).
 *
 * This function can be used to convert a buffer in-place.
 *
 * @param dest the destination buffer (array of triples)
 * @param src the source buffer
 */
void
pcm_pack_24(uint8_t *dest,
	    const int32_t *src, const int32_t *src_end) noexcept;

/**
 * Converts packed 24 bit samples (3 bytes per sample) to padded 24
 * bit samples (4 bytes per sample).
 *
 * @param dest the destination buffer
 * @param src the source buffer (array of triples)
 */
void
pcm_unpack_24(int32_t *dest,
	      const uint8_t *src, const uint8_t *src_end) noexcept;

/**
 * Like pcm_unpack_24(), but assume the source byte order is
 * big-endian.  The destination byte order ia always native.
 */
void
pcm_unpack_24be(int32_t *dest,
		const uint8_t *src, const uint8_t *src_end) noexcept;

#endif
