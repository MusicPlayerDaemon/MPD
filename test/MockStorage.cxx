// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
 * Mock storage implementation for testing.
 */

#include "MockStorage.hxx"
#include "util/StringCompare.hxx"

static std::unique_ptr<Storage>
CreateMockStorageURI([[maybe_unused]] EventLoop &event_loop, const char *uri)
{
	// Accept any URI starting with "mock://"
	if (!StringStartsWith(uri, "mock://"))
		return nullptr;

	return std::make_unique<MockStorage>(uri);
}

// Make prefixes array external linkage so it's accessible
const char *const mock_storage_prefixes[] = {
	"mock://",
	nullptr
};

const StoragePlugin mock_storage_plugin = {
	.name = "mock",
	.prefixes = mock_storage_prefixes,
	.create_uri = CreateMockStorageURI,
};
