// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OpusHead.hxx"
#include "util/ByteOrder.hxx"

#include <cstdint>

struct OpusHead {
	char signature[8];
	uint8_t version, channels;
	uint16_t pre_skip;
	uint32_t sample_rate;
	uint16_t output_gain;
	uint8_t channel_mapping;
};

bool
ScanOpusHeader(const void *data, size_t size, unsigned &channels_r,
	       signed &output_gain_r, unsigned &pre_skip_r)
{
	const auto *h = (const OpusHead *)data;
	if (size < 19 || (h->version & 0xf0) != 0)
		return false;

	output_gain_r = FromLE16S(h->output_gain);

	channels_r = h->channels;
	pre_skip_r = FromLE16(h->pre_skip);
	return true;
}
