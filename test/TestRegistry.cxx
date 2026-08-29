// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

// Test version of Registry.cxx that uses mock storage plugins

#include "storage/Registry.hxx"
#include "MockStorage.hxx"

// Define the storage_plugins array with only the mock plugin
constinit const StoragePlugin *const storage_plugins[] = {
	&mock_storage_plugin,
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
