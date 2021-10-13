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

#ifndef MPD_DECODER_DSDLIB_HXX
#define MPD_DECODER_DSDLIB_HXX

#include "util/ByteOrder.hxx"
#include "input/Offset.hxx"

#include <cstdint>

class TagHandler;
class DecoderClient;
class InputStream;

struct DsdId {
	char value[4];

	[[gnu::pure]]
	bool Equals(const char *s) const noexcept;
};

class DsdUint64 {
	uint32_t lo;
	uint32_t hi;

public:
	constexpr uint64_t Read() const {
		return (uint64_t(FromLE32(hi)) << 32) |
			uint64_t(FromLE32(lo));
	}
};

class DffDsdUint64 {
	uint32_t hi;
	uint32_t lo;

public:
	constexpr uint64_t Read() const {
		return (uint64_t(FromBE32(hi)) << 32) |
			uint64_t(FromBE32(lo));
	}
};

bool
dsdlib_skip_to(DecoderClient *client, InputStream &is,
	       offset_type offset);

bool
dsdlib_skip(DecoderClient *client, InputStream &is,
	    offset_type delta);

/**
 * Check if the sample frequency is a valid DSD frequency.
 **/
[[gnu::const]]
bool
dsdlib_valid_freq(uint32_t samplefreq) noexcept;

/**
 * Add tags from ID3 tag. All tags commonly found in the ID3 tags of
 * DSF and DSDIFF files are imported
 */
void
dsdlib_tag_id3(InputStream &is, TagHandler &handler,
	       offset_type tagoffset);

#endif
