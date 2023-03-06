// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FileSystem.hxx"
#include "AllocatedPath.hxx"
#include "Limits.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/SystemError.hxx"

#ifdef _WIN32
#include <handleapi.h> // for CloseHandle()
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for MoveFileEx()
#endif

#include <cerrno>

#include <fcntl.h>

void
RenameFile(Path oldpath, Path newpath)
{
#ifdef _WIN32
	if (!MoveFileEx(oldpath.c_str(), newpath.c_str(),
			MOVEFILE_REPLACE_EXISTING))
		throw MakeLastError("Failed to rename file");
#else
	if (rename(oldpath.c_str(), newpath.c_str()) < 0)
		throw MakeErrno("Failed to rename file");
#endif
}

AllocatedPath
ReadLink(Path path) noexcept
{
#ifdef _WIN32
	(void)path;
	errno = EINVAL;
	return nullptr;
#else
	char buffer[MPD_PATH_MAX];
	ssize_t size = readlink(path.c_str(), buffer, MPD_PATH_MAX);
	if (size < 0)
		return nullptr;
	if (size_t(size) >= MPD_PATH_MAX) {
		errno = ENOMEM;
		return nullptr;
	}
	return AllocatedPath::FromFS(std::string_view{buffer, size_t(size)});
#endif
}

void
TruncateFile(Path path)
{
#ifdef _WIN32
	HANDLE h = CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
			      TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL,
			      nullptr);
	if (h == INVALID_HANDLE_VALUE)
		throw FmtLastError("Failed to truncate {}", path);

	CloseHandle(h);
#else
	UniqueFileDescriptor fd;
	if (!fd.Open(path.c_str(), O_WRONLY|O_TRUNC))
		throw FmtErrno("Failed to truncate {}", path);
#endif
}

void
RemoveFile(Path path)
{
#ifdef _WIN32
	if (!DeleteFile(path.c_str()))
		throw FmtLastError("Failed to delete {}", path);
#else
	if (unlink(path.c_str()) < 0)
		throw FmtErrno("Failed to delete {}", path);
#endif
}
