// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INPUT_CACHE_CONFIG_HXX
#define MPD_INPUT_CACHE_CONFIG_HXX

#include <cstddef>

struct ConfigBlock;

struct InputCacheConfig {
	size_t size;

	explicit InputCacheConfig(const ConfigBlock &block);
};

#endif
