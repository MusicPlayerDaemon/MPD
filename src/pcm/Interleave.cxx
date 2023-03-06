// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Interleave.hxx"

#include <string.h>

static void
GenericPcmInterleave(std::byte *gcc_restrict dest,
		     std::span<const std::byte *const> src,
		     size_t n_frames, size_t sample_size) noexcept
{
	for (size_t frame = 0; frame < n_frames; ++frame) {
		for (size_t channel = 0; channel < src.size(); ++channel) {
			memcpy(dest, src[channel] + frame * sample_size,
			       sample_size);
			dest += sample_size;
		}
	}
}

template<typename T>
static void
PcmInterleaveStereo(T *gcc_restrict dest,
		    const T *gcc_restrict src1,
		    const T *gcc_restrict src2,
		    size_t n_frames) noexcept
{
	for (size_t i = 0; i != n_frames; ++i) {
		*dest++ = *src1++;
		*dest++ = *src2++;
	}
}

template<typename T>
static void
PcmInterleaveT(T *gcc_restrict dest,
	       const std::span<const T *const> src,
	       size_t n_frames) noexcept
{
	switch (src.size()) {
	case 2:
		PcmInterleaveStereo(dest, src[0], src[1], n_frames);
		return;
	}

	for (const auto *s : src) {
		auto *d = dest++;

		for (const auto *const s_end = s + n_frames;
		     s != s_end; ++s, d += src.size())
			*d = *s;
	}
}

static void
PcmInterleave16(int16_t *gcc_restrict dest,
		const std::span<const int16_t *const> src,
		size_t n_frames) noexcept
{
	PcmInterleaveT(dest, src, n_frames);
}

void
PcmInterleave32(int32_t *gcc_restrict dest,
		const std::span<const int32_t *const> src,
		size_t n_frames) noexcept
{
	PcmInterleaveT(dest, src, n_frames);
}

void
PcmInterleave(void *gcc_restrict dest,
	      std::span<const void *const> src,
	      size_t n_frames, size_t sample_size) noexcept
{
	switch (sample_size) {
	case 2:
		PcmInterleave16((int16_t *)dest,
				{(const int16_t *const*)src.data(), src.size()},
				n_frames);
		break;

	case 4:
		PcmInterleave32((int32_t *)dest,
				{(const int32_t *const*)src.data(), src.size()},
				n_frames);
		break;

	default:
		GenericPcmInterleave((std::byte *)dest,
				     {(const std::byte *const*)src.data(), src.size()},
				     n_frames, sample_size);
	}
}
