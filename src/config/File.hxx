// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIG_FILE_HXX
#define MPD_CONFIG_FILE_HXX

class Path;
struct ConfigData;

void
ReadConfigFile(ConfigData &data, Path path);

#endif
