// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "config/Block.hxx"
#include "config/Parser.hxx"

InputCacheConfig::InputCacheConfig(const ConfigBlock &block)
{
	const auto *size_param = block.GetBlockParam("size");
	if (size_param != nullptr)
		size = size_param->With([](const char *s){
			return ParseSize(s);
		});
}
