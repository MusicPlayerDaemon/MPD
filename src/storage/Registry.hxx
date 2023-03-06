// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_REGISTRY_HXX
#define MPD_STORAGE_REGISTRY_HXX

#include <memory>

struct StoragePlugin;
class Storage;
class EventLoop;

/**
 * nullptr terminated list of all storage plugins which were enabled at
 * compile time.
 */
extern const StoragePlugin *const storage_plugins[];

[[gnu::nonnull]] [[gnu::pure]]
const StoragePlugin *
GetStoragePluginByName(const char *name) noexcept;

[[gnu::nonnull]] [[gnu::pure]]
const StoragePlugin *
GetStoragePluginByUri(const char *uri) noexcept;

[[gnu::nonnull]]
std::unique_ptr<Storage>
CreateStorageURI(EventLoop &event_loop, const char *uri);

#endif
