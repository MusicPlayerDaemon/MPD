// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_RIFF_FORMAT_HXX
#define MPD_RIFF_FORMAT_HXX

#include <cstdint>

struct RiffFileHeader {
	char id[4];
	uint32_t size;
	char format[4];
};

static_assert(sizeof(RiffFileHeader) == 12);

struct RiffChunkHeader {
	char id[4];
	uint32_t size;
};

static_assert(sizeof(RiffChunkHeader) == 8);

struct RiffFmtChunk {
	static constexpr uint16_t TAG_PCM = 1;

	uint16_t tag;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

static_assert(sizeof(RiffFmtChunk) == 16);

#endif
