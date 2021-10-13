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

#ifndef PCM_BUFFER_HXX
#define PCM_BUFFER_HXX

#include "util/ReusableArray.hxx"

#include <cstdint>

/**
 * Manager for a temporary buffer which grows as needed.  We could
 * allocate a new buffer every time pcm_convert() is called, but that
 * would put too much stress on the allocator.
 */
class PcmBuffer {
	ReusableArray<uint8_t, 8192> buffer;

public:
	void Clear() noexcept {
		buffer.Clear();
	}

	/**
	 * Get the buffer, and guarantee a minimum size.  This buffer becomes
	 * invalid with the next Get() call.
	 *
	 * This function will never return nullptr, even if size is
	 * zero, because the PCM library uses the nullptr return value
	 * to signal "error".  An empty destination buffer is not
	 * always an error.
	 */
	[[gnu::malloc]] [[gnu::returns_nonnull]]
	void *Get(size_t size) noexcept;

	template<typename T>
	[[gnu::malloc]] [[gnu::returns_nonnull]]
	T *GetT(size_t n) noexcept {
		return (T *)Get(n * sizeof(T));
	}
};

#endif
