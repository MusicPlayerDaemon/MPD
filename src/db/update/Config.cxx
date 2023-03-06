// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Config.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"

UpdateConfig::UpdateConfig(const ConfigData &config)
{
#ifndef _WIN32
	follow_inside_symlinks =
		config.GetBool(ConfigOption::FOLLOW_INSIDE_SYMLINKS,
			       DEFAULT_FOLLOW_INSIDE_SYMLINKS);

	follow_outside_symlinks =
		config.GetBool(ConfigOption::FOLLOW_OUTSIDE_SYMLINKS,
			       DEFAULT_FOLLOW_OUTSIDE_SYMLINKS);
#else
	(void)config;
#endif
}
