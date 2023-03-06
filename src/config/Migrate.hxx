// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONFIG_MIGRATE_HXX
#define MPD_CONFIG_MIGRATE_HXX

struct ConfigData;

/**
 * Migrate deprecated #Config settings to new-style settings.
 */
void
Migrate(ConfigData &config) noexcept;

#endif
