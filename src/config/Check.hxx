// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIG_CHECK_HXX
#define MPD_CONFIG_CHECK_HXX

struct ConfigData;

/**
 * Call this function after all configuration has been evaluated.  It
 * checks for unused parameters, and logs warnings.
 */
void
Check(const ConfigData &config_data) noexcept;

#endif
