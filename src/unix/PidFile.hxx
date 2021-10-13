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

#ifndef MPD_PID_FILE_HXX
#define MPD_PID_FILE_HXX

#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "system/Error.hxx"

#include <cassert>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

class PidFile {
	int fd;

public:
	PidFile(const AllocatedPath &path):fd(-1) {
		if (path.IsNull())
			return;

		fd = OpenFile(path, O_WRONLY|O_CREAT|O_TRUNC, 0666).Steal();
		if (fd < 0) {
			const std::string utf8 = path.ToUTF8();
			throw FormatErrno("Failed to create pid file \"%s\"",
					  utf8.c_str());
		}
	}

	PidFile(const PidFile &) = delete;

	void Close() noexcept {
		if (fd < 0)
			return;

		close(fd);
	}

	void Delete(const AllocatedPath &path) noexcept {
		if (fd < 0) {
			assert(path.IsNull());
			return;
		}

		assert(!path.IsNull());

		close(fd);
		unlink(path.c_str());
	}

	void Write(pid_t pid) noexcept {
		if (fd < 0)
			return;

		char buffer[64];
		sprintf(buffer, "%lu\n", (unsigned long)pid);

		write(fd, buffer, strlen(buffer));
		close(fd);
	}

	void Write() noexcept {
		if (fd < 0)
			return;

		Write(getpid());
	}
};

[[gnu::pure]]
static inline pid_t
ReadPidFile(Path path) noexcept
{
	auto fd = OpenFile(path, O_RDONLY, 0);
	if (!fd.IsDefined())
		return -1;

	pid_t pid = -1;

	char buffer[32];
	auto nbytes = fd.Read(buffer, sizeof(buffer) - 1);
	if (nbytes > 0) {
		buffer[nbytes] = 0;

		char *endptr;
		auto value = strtoul(buffer, &endptr, 10);
		if (endptr > buffer)
			pid = value;
	}

	return pid;
}

#endif
