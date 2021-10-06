/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Registry.hxx"
#include "StoragePlugin.hxx"
#include "StorageInterface.hxx"
#include "plugins/LocalStorage.hxx"
#include "plugins/UdisksStorage.hxx"
#include "plugins/SmbclientStorage.hxx"
#include "plugins/NfsStorage.hxx"
#include "plugins/CurlStorage.hxx"
#include "config.h"

#include <string.h>

constexpr const StoragePlugin *storage_plugins[] = {
	&local_storage_plugin,
#ifdef ENABLE_SMBCLIENT
	&smbclient_storage_plugin,
#endif
#ifdef ENABLE_UDISKS
	&udisks_storage_plugin,
#endif
#ifdef ENABLE_NFS
	&nfs_storage_plugin,
#endif
#ifdef ENABLE_WEBDAV
	&curl_storage_plugin,
#endif
	nullptr
};

const StoragePlugin *
GetStoragePluginByName(const char *name) noexcept
{
	for (auto i = storage_plugins; *i != nullptr; ++i) {
		const StoragePlugin &plugin = **i;
		if (strcmp(plugin.name, name) == 0)
			return *i;
	}

	return nullptr;
}

const StoragePlugin *
GetStoragePluginByUri(const char *uri) noexcept
{
	for (auto i = storage_plugins; *i != nullptr; ++i) {
		const StoragePlugin &plugin = **i;
		if (plugin.SupportsUri(uri))
			return *i;
	}

	return nullptr;
}

std::unique_ptr<Storage>
CreateStorageURI(EventLoop &event_loop, const char *uri)
{
	for (auto i = storage_plugins; *i != nullptr; ++i) {
		const StoragePlugin &plugin = **i;

		if (plugin.create_uri == nullptr || !plugin.SupportsUri(uri))
			continue;

		auto storage = plugin.create_uri(event_loop, uri);
		if (storage != nullptr)
			return storage;
	}

	return nullptr;
}
