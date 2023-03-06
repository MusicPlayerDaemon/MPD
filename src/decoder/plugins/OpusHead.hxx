// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OPUS_HEAD_HXX
#define MPD_OPUS_HEAD_HXX

#include <cstddef>

bool
ScanOpusHeader(const void *data, size_t size, unsigned &channels_r,
	       signed &output_gain_r, unsigned &pre_skip_r);

#endif
