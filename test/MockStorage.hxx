// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/**
 * Mock storage implementation for testing.
 *
 * This provides a minimal in-memory storage that can be mounted and
 * unmounted during tests without requiring actual filesystem or network
 * resources.
 */

#ifndef MPD_MOCK_STORAGE_HXX
#define MPD_MOCK_STORAGE_HXX

#include "storage/StorageInterface.hxx"
#include "storage/StoragePlugin.hxx"
#include "storage/FileInfo.hxx"
#include "fs/AllocatedPath.hxx"
#include "input/InputStream.hxx"

/**
 * A minimal mock storage implementation that stores only its URI.
 *
 * This is sufficient for testing state file read/write operations,
 * which only need to serialize and deserialize mount points.
 */
class MockStorage final : public Storage {
	std::string uri;

public:
	explicit MockStorage(std::string_view _uri) noexcept
		: uri(_uri) {}

	/* virtual methods from class Storage */
	StorageFileInfo GetInfo(std::string_view, bool) override {
		throw std::runtime_error("Not implemented in mock");
	}

	std::unique_ptr<StorageDirectoryReader> OpenDirectory(std::string_view) override {
		throw std::runtime_error("Not implemented in mock");
	}

	std::string MapUTF8(std::string_view) const noexcept override {
		return uri;
	}

	AllocatedPath MapFS(std::string_view) const noexcept override {
		return nullptr;
	}

	std::string_view MapToRelativeUTF8(std::string_view) const noexcept override {
		return {};
	}

	InputStreamPtr OpenFile(std::string_view, Mutex &) override {
		return nullptr;
	}
};

/**
 * Storage plugin for creating mock storage instances.
 */
extern const StoragePlugin mock_storage_plugin;

#endif
