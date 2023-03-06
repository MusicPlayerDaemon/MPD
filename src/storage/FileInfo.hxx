// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_FILE_INFO_HXX
#define MPD_STORAGE_FILE_INFO_HXX

#include <chrono>

#include <cstdint>

struct StorageFileInfo {
	enum class Type : uint8_t {
		OTHER,
		REGULAR,
		DIRECTORY,
	};

	Type type;

	/**
	 * The file size in bytes.  Only valid for #Type::REGULAR.
	 */
	uint64_t size;

	/**
	 * The modification time.  A negative value means unknown /
	 * not applicable.
	 */
	std::chrono::system_clock::time_point mtime;

	/**
	 * Device id and inode number.  0 means unknown / not
	 * applicable.
	 */
	uint64_t device, inode;

	StorageFileInfo() = default;

	explicit constexpr StorageFileInfo(Type _type)
		:type(_type),
		 size(0),
		 mtime(std::chrono::system_clock::time_point::min()),
		 device(0), inode(0) {}

	constexpr bool IsRegular() const {
		return type == Type::REGULAR;
	}

	constexpr bool IsDirectory() const {
		return type == Type::DIRECTORY;
	}
};

#endif
