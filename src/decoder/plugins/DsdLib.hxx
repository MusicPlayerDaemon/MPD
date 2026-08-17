// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Offset.hxx"
#include "util/FixedString.hxx"

#include <cstdint>
#include <string_view>

class TagHandler;
class DecoderClient;
class InputStream;

struct DsdId {
	FixedString<4> value;

	constexpr bool Equals(std::string_view other) const noexcept {
		return other == std::string_view{value};
	}
};

/**
 * Check if the sample frequency is a valid DSD frequency.
 **/
[[gnu::const]]
bool
dsdlib_valid_freq(uint32_t samplefreq) noexcept;

/**
 * Add tags from ID3 tag. All tags commonly found in the ID3 tags of
 * DSF and DSDIFF files are imported
 *
 * @return true on success (even if the ID3 tag was empty), false on
 * error
 */
bool
dsdlib_tag_id3(DecoderClient *client, InputStream &is,
	       TagHandler &handler, offset_type tagoffset);
