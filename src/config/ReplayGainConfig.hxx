// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

struct ConfigData;

struct ReplayGainConfig {
	static constexpr bool DEFAULT_LIMIT = true;

	float preamp = 1.0;

	float missing_preamp = 1.0;

	bool limit = DEFAULT_LIMIT;

	ReplayGainConfig() = default;

	explicit ReplayGainConfig(const ConfigData &config);
};
