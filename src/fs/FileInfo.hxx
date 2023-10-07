// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Path.hxx"

#ifdef _WIN32
#include "time/FileTime.hxx"
#else
#include <sys/stat.h>
#endif

#include <chrono>
#include <cstdint>

class FileDescriptor;

class FileInfo {
	friend bool GetFileInfo(Path path, FileInfo &info,
				bool follow_symlinks) noexcept;
#ifdef _WIN32
	friend bool GetFileInfoByHandle(HANDLE handle, FileInfo &info) noexcept;
#endif

#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
#else
	struct stat st;
#endif

public:
	constexpr FileInfo() noexcept = default;

	explicit FileInfo(Path path, bool follow_symlinks=true);

#ifdef _WIN32
	explicit FileInfo(HANDLE handle);
#else
	explicit FileInfo(FileDescriptor fd);
#endif

	constexpr bool IsRegular() const noexcept {
#ifdef _WIN32
		return (data.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE)) == 0;
#else
		return S_ISREG(st.st_mode);
#endif
	}

	constexpr bool IsDirectory() const noexcept {
#ifdef _WIN32
		return data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else
		return S_ISDIR(st.st_mode);
#endif
	}

	constexpr uint_least64_t GetSize() const noexcept {
#ifdef _WIN32
		return ConstructUint64(data.nFileSizeLow, data.nFileSizeHigh);
#else
		return st.st_size;
#endif
	}

	[[gnu::pure]]
	std::chrono::system_clock::time_point GetModificationTime() const noexcept {
#ifdef _WIN32
		return FileTimeToChrono(data.ftLastWriteTime);
#else
		return std::chrono::system_clock::from_time_t(st.st_mtime);
#endif
	}

#ifndef _WIN32
	constexpr uid_t GetUid() const noexcept {
		return st.st_uid;
	}

	constexpr mode_t GetMode() const noexcept {
		return st.st_mode;
	}

	constexpr dev_t GetDevice() const noexcept {
		return st.st_dev;
	}

	constexpr ino_t GetInode() const noexcept {
		return st.st_ino;
	}
#endif
};

inline bool
GetFileInfo(Path path, FileInfo &info, bool follow_symlinks=true) noexcept
{
#ifdef _WIN32
	(void)follow_symlinks;
	return GetFileAttributesEx(path.c_str(), GetFileExInfoStandard,
				   &info.data);
#else
	int ret = follow_symlinks
		? stat(path.c_str(), &info.st)
		: lstat(path.c_str(), &info.st);
	return ret == 0;
#endif
}

#ifdef _WIN32

bool
GetFileInfoByHandle(HANDLE handle, FileInfo &info) noexcept;

#endif
