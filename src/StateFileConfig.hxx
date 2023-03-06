// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STATE_FILE_CONFIG_HXX
#define MPD_STATE_FILE_CONFIG_HXX

#include "fs/AllocatedPath.hxx"
#include "event/Chrono.hxx"

struct ConfigData;

struct StateFileConfig {
	static constexpr Event::Duration DEFAULT_INTERVAL = std::chrono::minutes(2);

	AllocatedPath path;

	Event::Duration interval;

	bool restore_paused;

	explicit StateFileConfig(const ConfigData &config);

	bool IsEnabled() const noexcept {
		return !path.IsNull();
	}
};

#endif
