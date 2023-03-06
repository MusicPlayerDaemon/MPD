// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPDATE_CONFIG_HXX
#define MPD_UPDATE_CONFIG_HXX

struct ConfigData;

struct UpdateConfig {
#ifndef _WIN32
	static constexpr bool DEFAULT_FOLLOW_INSIDE_SYMLINKS = true;
	static constexpr bool DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true;

	bool follow_inside_symlinks = DEFAULT_FOLLOW_INSIDE_SYMLINKS;
	bool follow_outside_symlinks = DEFAULT_FOLLOW_OUTSIDE_SYMLINKS;
#endif

	explicit UpdateConfig(const ConfigData &config);
};

#endif
