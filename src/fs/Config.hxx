// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_CONFIG_HXX
#define MPD_FS_CONFIG_HXX

struct ConfigData;

/**
 * Performs global one-time initialization of this class.
 *
 * Throws std::runtime_error on error.
 */
void
ConfigureFS(const ConfigData &config);

void
DeinitFS() noexcept;

#endif
