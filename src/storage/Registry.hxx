/*
 * Copyright 2003-2019 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_STORAGE_REGISTRY_HXX
#define MPD_STORAGE_REGISTRY_HXX

#include "util/Compiler.h"

#include <memory>

struct StoragePlugin;
class Storage;
class EventLoop;

/**
 * nullptr terminated list of all storage plugins which were enabled at
 * compile time.
 */
extern const StoragePlugin *const storage_plugins[];

gcc_nonnull_all gcc_pure
const StoragePlugin *
GetStoragePluginByName(const char *name) noexcept;

gcc_nonnull_all
std::unique_ptr<Storage>
CreateStorageURI(EventLoop &event_loop, const char *uri);

#endif
