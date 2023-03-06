// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FS_FILE_INFO_HXX
#define MPD_FS_FILE_INFO_HXX

#include "Path.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"

#ifdef _WIN32
#include "time/FileTime.hxx"
#else
#include <sys/stat.h>
#endif

#include <chrono>
#include <cstdint>

class FileInfo {
	friend bool GetFileInfo(Path path, FileInfo &info,
				bool follow_symlinks);
	friend class FileReader;

#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
#else
	struct stat st;
#endif

public:
	FileInfo() = default;

	FileInfo(Path path, bool follow_symlinks=true) {
		if (!GetFileInfo(path, *this, follow_symlinks)) {
#ifdef _WIN32
			throw FmtLastError("Failed to access {}", path);
#else
			throw FmtErrno("Failed to access {}", path);
#endif
		}
	}

	bool IsRegular() const {
#ifdef _WIN32
		return (data.dwFileAttributes &
			(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE)) == 0;
#else
		return S_ISREG(st.st_mode);
#endif
	}

	bool IsDirectory() const {
#ifdef _WIN32
		return data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else
		return S_ISDIR(st.st_mode);
#endif
	}

	uint64_t GetSize() const {
#ifdef _WIN32
		return ConstructUint64(data.nFileSizeLow, data.nFileSizeHigh);
#else
		return st.st_size;
#endif
	}

	std::chrono::system_clock::time_point GetModificationTime() const {
#ifdef _WIN32
		return FileTimeToChrono(data.ftLastWriteTime);
#else
		return std::chrono::system_clock::from_time_t(st.st_mtime);
#endif
	}

#ifndef _WIN32
	uid_t GetUid() const {
		return st.st_uid;
	}

	mode_t GetMode() const {
		return st.st_mode;
	}

	dev_t GetDevice() const {
		return st.st_dev;
	}

	ino_t GetInode() const {
		return st.st_ino;
	}
#endif
};

inline bool
GetFileInfo(Path path, FileInfo &info, bool follow_symlinks=true)
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

#endif
