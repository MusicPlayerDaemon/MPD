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
#include "FileReader.hxx"
#include "system/fd_util.h"
#include "util/Error.hxx"

#ifdef WIN32

FileReader::FileReader(Path _path, Error &error)
	:path(_path),
	 handle(CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
			   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			   nullptr))
{
	if (handle == INVALID_HANDLE_VALUE)
		error.FormatLastError("Failed to open %s", path.c_str());
}

size_t
FileReader::Read(void *data, size_t size, Error &error)
{
	assert(IsDefined());

	DWORD nbytes;
	if (!ReadFile(handle, data, size, &nbytes, nullptr)) {
		error.FormatLastError("Failed to read from %s", path.c_str());
		nbytes = 0;
	}

	return nbytes;
}

void
FileReader::Close()
{
	assert(IsDefined());

	CloseHandle(handle);
}

#else

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

FileReader::FileReader(Path _path, Error &error)
	:path(_path),
	 fd(open_cloexec(path.c_str(),
			 O_RDONLY,
			 0))
{
	if (fd < 0)
		error.FormatErrno("Failed to open %s", path.c_str());
}

size_t
FileReader::Read(void *data, size_t size, Error &error)
{
	assert(IsDefined());

	ssize_t nbytes = read(fd, data, size);
	if (nbytes < 0) {
		error.FormatErrno("Failed to read from %s", path.c_str());
		nbytes = 0;
	}

	return nbytes;
}

void
FileReader::Close()
{
	assert(IsDefined());

	close(fd);
	fd = -1;
}

#endif
