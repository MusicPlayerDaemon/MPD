// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_LOCAL_HXX
#define MPD_STORAGE_LOCAL_HXX

#include <memory>

struct StoragePlugin;
class Storage;
class Path;

extern const StoragePlugin local_storage_plugin;

std::unique_ptr<Storage>
CreateLocalStorage(Path base_fs);

#endif
