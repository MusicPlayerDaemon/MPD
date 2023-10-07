// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileInfo.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"

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
