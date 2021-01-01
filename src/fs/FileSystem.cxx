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

#include "FileSystem.hxx"
#include "AllocatedPath.hxx"
#include "Limits.hxx"
#include "system/Error.hxx"

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
ReadLink(Path path)
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
		throw FormatLastError("Failed to truncate %s", path.c_str());

	CloseHandle(h);
#else
	UniqueFileDescriptor fd;
	if (!fd.Open(path.c_str(), O_WRONLY|O_TRUNC))
		throw FormatErrno("Failed to truncate %s", path.c_str());
#endif
}

void
RemoveFile(Path path)
{
#ifdef _WIN32
	if (!DeleteFile(path.c_str()))
		throw FormatLastError("Failed to delete %s", path.c_str());
#else
	if (unlink(path.c_str()) < 0)
		throw FormatErrno("Failed to delete %s", path.c_str());
#endif
}
