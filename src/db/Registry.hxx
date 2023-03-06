// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_REGISTRY_HXX
#define MPD_DATABASE_REGISTRY_HXX

struct DatabasePlugin;

/**
 * nullptr terminated list of all database plugins which were enabled at
 * compile time.
 */
extern const DatabasePlugin *const database_plugins[];

[[gnu::pure]]
const DatabasePlugin *
GetDatabasePluginByName(const char *name) noexcept;

#endif
