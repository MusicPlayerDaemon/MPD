// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "lib/fmt/PathFormatter.hxx"
#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/Unsafe.hxx"
#include "io/FileDescriptor.hxx"
#include "util/SpanCast.hxx"

#include <cassert>

#include <fcntl.h>

class PidFile {
	FileDescriptor fd;

public:
	PidFile(const AllocatedPath &path):fd(-1) {
		if (path.IsNull())
			return;

		fd = OpenFile(path, O_WRONLY|O_CREAT|O_TRUNC, 0666).Release();
		if (!fd.IsDefined())
			throw FmtErrno("Failed to create pid file {:?}",
				       path);
	}

	PidFile(const PidFile &) = delete;

	void Close() noexcept {
		if (!fd.IsDefined())
			return;

		fd.Close();
	}

	void Delete(const AllocatedPath &path) noexcept {
		if (!fd.IsDefined()) {
			assert(path.IsNull());
			return;
		}

		assert(!path.IsNull());

		fd.Close();
		unlink(path.c_str());
	}

	void Write(pid_t pid) noexcept {
		if (!fd.IsDefined())
			return;

		char buffer[32];
		(void)fd.Write(AsBytes(FmtUnsafeSV(buffer, "{}\n", pid)));
		fd.Close();
	}

	void Write() noexcept {
		if (!fd.IsDefined())
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
	auto nbytes = fd.Read(std::as_writable_bytes(std::span{buffer, sizeof(buffer) - 1}));
	if (nbytes > 0) {
		buffer[nbytes] = 0;

		char *endptr;
		auto value = strtoul(buffer, &endptr, 10);
		if (endptr > buffer)
			pid = value;
	}

	return pid;
}
