// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OPUS_TAGS_HXX
#define MPD_OPUS_TAGS_HXX

#include <cstddef>

struct ReplayGainInfo;
class TagHandler;

bool
ScanOpusTags(const void *data, size_t size,
	     ReplayGainInfo *rgi,
	     TagHandler &handler) noexcept;

#endif
