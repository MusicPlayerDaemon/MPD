// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileInfo.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"

#ifdef _WIN32
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for FILE_*_INFO
#else
#include "io/FileDescriptor.hxx"
#endif

FileInfo::FileInfo(Path path, bool follow_symlinks)
{
	if (!GetFileInfo(path, *this, follow_symlinks)) {
#ifdef _WIN32
		throw FmtLastError("Failed to access {}", path);
#else
		throw FmtErrno("Failed to access {}", path);
#endif
	}
}

#ifdef _WIN32

FileInfo::FileInfo(HANDLE handle)
{
	if (!GetFileInfoByHandle(handle, *this))
		throw MakeLastError("Failed to access file");
}

bool
GetFileInfoByHandle(HANDLE handle, FileInfo &info) noexcept
{
	FILE_BASIC_INFO basic;
	FILE_STANDARD_INFO standard;

	if (!GetFileInformationByHandleEx(handle, FileBasicInfo,
					  &basic, sizeof(basic)) ||
	    !GetFileInformationByHandleEx(handle, FileStandardInfo,
					  &standard, sizeof(standard)))
		return false;

	info.data.dwFileAttributes = basic.FileAttributes;
	info.data.ftCreationTime.dwLowDateTime = basic.CreationTime.LowPart;
	info.data.ftCreationTime.dwHighDateTime = basic.CreationTime.HighPart;
	info.data.ftLastAccessTime.dwLowDateTime = basic.LastAccessTime.LowPart;
	info.data.ftLastAccessTime.dwHighDateTime = basic.LastAccessTime.HighPart;
	info.data.ftLastWriteTime.dwLowDateTime = basic.LastWriteTime.LowPart;
	info.data.ftLastWriteTime.dwHighDateTime = basic.LastWriteTime.HighPart;

	info.data.nFileSizeLow = standard.EndOfFile.LowPart;
	info.data.nFileSizeHigh = standard.EndOfFile.HighPart;

	return true;
}

#else

FileInfo::FileInfo(FileDescriptor fd)
{
	if (fstat(fd.Get(), &st) < 0)
		throw MakeErrno("Failed to access file");
}

#endif // _WIN32
