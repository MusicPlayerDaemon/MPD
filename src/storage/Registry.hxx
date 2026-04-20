// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <memory>
#include <string_view>

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

[[gnu::pure]]
const StoragePlugin *
GetStoragePluginByUri(std::string_view uri) noexcept;

std::unique_ptr<Storage>
CreateStorageURI(EventLoop &event_loop, std::string_view uri);
