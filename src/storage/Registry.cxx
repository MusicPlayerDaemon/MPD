// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

constinit const StoragePlugin *const storage_plugins[] = {
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
