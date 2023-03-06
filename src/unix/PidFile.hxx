// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PID_FILE_HXX
#define MPD_PID_FILE_HXX

#include "lib/fmt/PathFormatter.hxx"
#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"

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
		if (fd < 0)
			throw FmtErrno("Failed to create pid file \"{}\"",
				       path);
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

		const auto s = FmtBuffer<64>("{}\n", pid);

		write(fd, s.c_str(), strlen(s.c_str()));
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
