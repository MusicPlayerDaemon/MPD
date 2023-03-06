// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TEST_CONFIG_GLUE_HXX
#define MPD_TEST_CONFIG_GLUE_HXX

#include "config/File.hxx"
#include "config/Migrate.hxx"
#include "config/Data.hxx"
#include "fs/Path.hxx"

inline ConfigData
AutoLoadConfigFile(Path path)
{
	ConfigData data;

	if (!path.IsNull()) {
		ReadConfigFile(data, path);
		Migrate(data);
	}

	return data;
}

#endif
