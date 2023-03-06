// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_MEMORY_DIRECTORY_READER_HXX
#define MPD_STORAGE_MEMORY_DIRECTORY_READER_HXX

#include "StorageInterface.hxx"
#include "FileInfo.hxx"

#include <string>
#include <forward_list>

/**
 * A #StorageDirectoryReader implementation that returns directory
 * entries from a memory allocation.
 */
class MemoryStorageDirectoryReader final : public StorageDirectoryReader {
public:
	struct Entry {
		std::string name;

		StorageFileInfo info;

		template<typename N>
		explicit Entry(N &&_name):name(std::forward<N>(_name)) {}
	};

	typedef std::forward_list<Entry> List;

private:
	List entries;

	bool first;

public:
	MemoryStorageDirectoryReader()
		:first(true) {}

	MemoryStorageDirectoryReader(MemoryStorageDirectoryReader &&src)
		:entries(std::move(src.entries)), first(src.first) {}

	MemoryStorageDirectoryReader(List &&_entries)
		:entries(std::move(_entries)), first(true) {}

	/* virtual methods from class StorageDirectoryReader */
	const char *Read() noexcept override;
	StorageFileInfo GetInfo(bool follow) override;
};

#endif
