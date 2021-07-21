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
