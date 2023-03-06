// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef PCM_BUFFER_HXX
#define PCM_BUFFER_HXX

#include "util/ReusableArray.hxx"

#include <cstddef>

/**
 * Manager for a temporary buffer which grows as needed.  We could
 * allocate a new buffer every time pcm_convert() is called, but that
 * would put too much stress on the allocator.
 */
class PcmBuffer {
	ReusableArray<std::byte, 8192> buffer;

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
