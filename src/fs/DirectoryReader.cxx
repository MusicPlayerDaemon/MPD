// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DirectoryReader.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"

#ifdef _WIN32

#include <handleapi.h> // for INVALID_HANDLE_VALUE

DirectoryReader::DirectoryReader(Path dir)
	:handle(FindFirstFile(MakeWildcardPath(dir.c_str()), &data))
{
	if (handle == INVALID_HANDLE_VALUE)
		throw FmtLastError("Failed to open {}", dir);
}

#else

DirectoryReader::DirectoryReader(Path dir)
	:dirp(opendir(dir.c_str()))
{
	if (dirp == nullptr)
		throw FmtErrno("Failed to open {}", dir);
}

#endif
