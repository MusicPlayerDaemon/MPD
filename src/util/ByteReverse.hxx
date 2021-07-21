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
