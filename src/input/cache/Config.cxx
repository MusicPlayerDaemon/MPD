// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "config/Block.hxx"
#include "config/Parser.hxx"

static constexpr size_t KILOBYTE = 1024;
static constexpr size_t MEGABYTE = 1024 * KILOBYTE;

InputCacheConfig::InputCacheConfig(const ConfigBlock &block)
{
	size = 256 * MEGABYTE;
	const auto *size_param = block.GetBlockParam("size");
	if (size_param != nullptr)
		size = size_param->With([](const char *s){
			return ParseSize(s);
		});
}
