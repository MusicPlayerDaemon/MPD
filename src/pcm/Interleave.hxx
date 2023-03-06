// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_INTERLEAVE_HXX
#define MPD_PCM_INTERLEAVE_HXX

#include "util/Compiler.h"

#include <cstddef>
#include <cstdint>
#include <span>

/**
 * Interleave planar PCM samples from #src to #dest.
 */
void
PcmInterleave(void *gcc_restrict dest, std::span<const void *const> src,
	      size_t n_frames, size_t sample_size) noexcept;

/**
 * A variant of PcmInterleave() that assumes 32 bit samples (4 bytes
 * per sample).
 */
void
PcmInterleave32(int32_t *gcc_restrict dest,
		std::span<const int32_t *const> src,
		size_t n_frames) noexcept;

static inline void
PcmInterleaveFloat(float *gcc_restrict dest,
		   std::span<const float *const> src,
		   size_t n_frames) noexcept
{
	PcmInterleave32((int32_t *)dest,
			std::span<const int32_t *const>((const int32_t *const*)src.data(),
							src.size()),
			n_frames);
}

#endif
