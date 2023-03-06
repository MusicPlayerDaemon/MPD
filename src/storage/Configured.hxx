// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_CONFIG_HXX
#define MPD_STORAGE_CONFIG_HXX

#include <memory>

struct ConfigData;
class Storage;
class EventLoop;

/**
 * Read storage configuration settings and create a #Storage instance
 * from it.  Returns nullptr if no storage is configured.
 *
 * Throws #std::runtime_error on error.
 */
std::unique_ptr<Storage>
CreateConfiguredStorage(const ConfigData &config, EventLoop &event_loop);

/**
 * Returns true if there is configuration for a #Storage instance.
 */
[[gnu::const]]
bool
IsStorageConfigured(const ConfigData &config) noexcept;

#endif
