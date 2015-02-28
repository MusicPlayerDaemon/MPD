/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "check.h"
#include "Path.hxx"
#include "util/Error.hxx"

#include <stdint.h>

#ifdef WIN32
#include <fileapi.h>
#else
#include <sys/stat.h>
#endif

#ifdef WIN32

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

#endif

class FileInfo {
	friend bool GetFileInfo(Path path, FileInfo &info,
				bool follow_symlinks);
	friend bool GetFileInfo(Path path, FileInfo &info,
				Error &error);

#ifdef WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
#else
	struct stat st;
#endif

public:
	bool IsRegular() const {
#ifdef WIN32
		return data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL;
#else
		return S_ISREG(st.st_mode);
#endif
	}

	bool IsDirectory() const {
#ifdef WIN32
		return data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else
		return S_ISDIR(st.st_mode);
#endif
	}

	uint64_t GetSize() const {
#ifdef WIN32
		return ConstructUint64(data.nFileSizeLow, data.nFileSizeHigh);
#else
		return st.st_size;
#endif
	}

	time_t GetModificationTime() const {
#ifdef WIN32
		return FileTimeToTimeT(data.ftLastWriteTime);
#else
		return st.st_mtime;
#endif
	}

#ifndef WIN32
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
#ifdef WIN32
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

inline bool
GetFileInfo(Path path, FileInfo &info, bool follow_symlinks, Error &error)
{
	bool success = GetFileInfo(path, info, follow_symlinks);
	if (!success) {
		const auto path_utf8 = path.ToUTF8();
#ifdef WIN32
		error.FormatLastError("Failed to access %s",
				      path_utf8.c_str());
#else
		error.FormatErrno("Failed to access %s", path_utf8.c_str());
#endif
	}

	return success;
}

inline bool
GetFileInfo(Path path, FileInfo &info, Error &error)
{
	return GetFileInfo(path, info, true, error);
}

#endif
