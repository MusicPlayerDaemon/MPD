// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FILTER_FACTORY_HXX
#define MPD_FILTER_FACTORY_HXX

#include <memory>

struct ConfigData;
class PreparedFilter;

class FilterFactory {
	const ConfigData &config;

public:
	explicit FilterFactory(const ConfigData &_config) noexcept
		:config(_config) {}

	std::unique_ptr<PreparedFilter> MakeFilter(const char *name);
};

#endif
