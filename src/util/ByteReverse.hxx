// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_BYTE_REVERSE_HXX
#define MPD_BYTE_REVERSE_HXX

#include <cstddef>
#include <cstdint>

/**
 * Reverse the bytes in each 16 bit "frame".  This function can be
 * used for in-place operation.
 */
void
reverse_bytes_16(uint16_t *dest,
		 const uint16_t *src, const uint16_t *src_end) noexcept;

/**
 * Reverse the bytes in each 32 bit "frame".  This function can be
 * used for in-place operation.
 */
void
reverse_bytes_32(uint32_t *dest,
		 const uint32_t *src, const uint32_t *src_end) noexcept;

/**
 * Reverse the bytes in each 64 bit "frame".  This function can be
 * used for in-place operation.
 */
void
reverse_bytes_64(uint64_t *dest,
		 const uint64_t *src, const uint64_t *src_end) noexcept;

/**
 * Reverse the bytes in each "frame".  This function cannot be used
 * for in-place operation.
 */
void
reverse_bytes(uint8_t *dest, const uint8_t *src, const uint8_t *src_end,
	      size_t frame_size) noexcept;

#endif
