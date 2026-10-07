// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/ByteSizes.hxx"

#include <cstddef>

struct ConfigBlock;

struct InputCacheConfig {
	std::size_t size = 256_MiB;

	explicit InputCacheConfig(const ConfigBlock &block);
};
