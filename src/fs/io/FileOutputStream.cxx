/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "FileOutputStream.hxx"
#include "fs/FileSystem.hxx"
#include "system/fd_util.h"
#include "util/Error.hxx"

#ifdef WIN32

FileOutputStream::FileOutputStream(Path _path, Error &error)
	:path(_path),
	 handle(CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
			   TRUNCATE_EXISTING,
			   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,
			   nullptr))
{
	if (handle == INVALID_HANDLE_VALUE)
		error.FormatLastError("Failed to create %s", path.c_str());
}

bool
FileOutputStream::Write(const void *data, size_t size, Error &error)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!WriteFile(handle, data, size, &nbytes, nullptr)) {
		error.FormatLastError("Failed to write to %s", path.c_str());
		return false;
	}

	if (size_t(nbytes) != size) {
		error.FormatLastError(ERROR_DISK_FULL,
				      "Failed to write to %s", path.c_str());
		return false;
	}

	return true;
}

bool
FileOutputStream::Commit(gcc_unused Error &error)
{
	assert(IsDefined());

	CloseHandle(handle);
	handle = INVALID_HANDLE_VALUE;
	return true;
}

void
FileOutputStream::Cancel()
{
	assert(IsDefined());

	CloseHandle(handle);
	handle = INVALID_HANDLE_VALUE;
	RemoveFile(path);
}

#else

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

FileOutputStream::FileOutputStream(Path _path, Error &error)
	:path(_path),
	 fd(open_cloexec(path.c_str(),
			 O_WRONLY|O_CREAT|O_TRUNC,
			 0666))
{
	if (fd < 0)
		error.FormatErrno("Failed to create %s", path.c_str());
}

bool
FileOutputStream::Write(const void *data, size_t size, Error &error)
{
	assert(IsDefined());

	ssize_t nbytes = write(fd, data, size);
	if (nbytes < 0) {
		error.FormatErrno("Failed to write to %s", path.c_str());
		return false;
	} else if ((size_t)nbytes < size) {
		error.FormatErrno(ENOSPC,
				  "Failed to write to %s", path.c_str());
		return false;
	}

	return true;
}

bool
FileOutputStream::Commit(Error &error)
{
	assert(IsDefined());

	bool success = close(fd) == 0;
	fd = -1;
	if (!success)
		error.FormatErrno("Failed to commit %s", path.c_str());

	return success;
}

void
FileOutputStream::Cancel()
{
	assert(IsDefined());

	close(fd);
	fd = -1;

	RemoveFile(path);
}

#endif
