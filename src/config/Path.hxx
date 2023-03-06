// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIG_PATH_HXX
#define MPD_CONFIG_PATH_HXX

struct ConfigData;
class AllocatedPath;

void
InitPathParser(const ConfigData &config) noexcept;

/**
 * Throws #std::runtime_error on error.
 */
AllocatedPath
ParsePath(const char *path);

#endif
