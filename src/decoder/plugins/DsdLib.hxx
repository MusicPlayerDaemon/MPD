// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
