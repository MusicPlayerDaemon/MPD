/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef MPD_FS_FILE_INFO_HXX
#define MPD_FS_FILE_INFO_HXX

#include "Path.hxx"
#include "system/Error.hxx"

#ifdef _WIN32
#include <fileapi.h>
#else
#include <sys/stat.h>
#endif

#include <chrono>

#include <stdint.h>

#ifdef _WIN32

static inline constexpr uint64_t
ConstructUint64(DWORD lo, DWORD hi)
{
	return uint64_t(lo) | (uint64_t(hi) << 32);
}

static constexpr time_t
FileTimeToTimeT(FILETIME ft)
{
	return (ConstructUint64(ft.dwLowDateTime, ft.dwHighDateTime)
		- 116444736000000000) / 10000000;
}

static std::chrono::system_clock::time_point
FileTimeToChrono(FILETIME ft)
{
	// TODO: eliminate the time_t roundtrip, preserve sub-second resolution
	return std::chrono::system_clock::from_time_t(FileTimeToTimeT(ft));
}

#endif

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
			throw FormatLastError("Failed to access %s",
					      path.ToUTF8().c_str());
#else
			throw FormatErrno("Failed to access %s",
					  path.ToUTF8().c_str());
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
