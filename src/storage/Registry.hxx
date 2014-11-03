/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "check.h"
#include "Compiler.h"

struct StoragePlugin;
class Storage;
class Error;
class EventLoop;

/**
 * nullptr terminated list of all storage plugins which were enabled at
 * compile time.
 */
extern const StoragePlugin *const storage_plugins[];

gcc_nonnull_all gcc_pure
const StoragePlugin *
GetStoragePluginByName(const char *name);

gcc_nonnull_all gcc_malloc
Storage *
CreateStorageURI(EventLoop &event_loop, const char *uri, Error &error);

#endif
