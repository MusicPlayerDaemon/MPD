// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"

#include <fcntl.h>

UniqueFileDescriptor
OpenReadOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_RDONLY|flags))
		throw FmtErrno("Failed to open '{}'", path);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_WRONLY|flags))
		throw FmtErrno("Failed to open '{}'", path);

	return fd;
}

#ifndef _WIN32

UniqueFileDescriptor
OpenDirectory(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_DIRECTORY|O_RDONLY|flags))
		throw FmtErrno("Failed to open '{}'", path);

	return fd;
}

#endif

#ifdef __linux__

UniqueFileDescriptor
OpenPath(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_PATH|flags))
		throw FmtErrno("Failed to open '{}'", path);

	return fd;
}

UniqueFileDescriptor
OpenPath(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_PATH|flags))
		throw FmtErrno("Failed to open '{}'", name);

	return fd;
}

UniqueFileDescriptor
OpenReadOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_RDONLY|flags))
		throw FmtErrno("Failed to open '{}'", name);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_WRONLY|flags))
		throw FmtErrno("Failed to open '{}'", name);

	return fd;
}

UniqueFileDescriptor
OpenDirectory(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_DIRECTORY|O_RDONLY|flags))
		throw FmtErrno("Failed to open '{}'", name);

	return fd;
}

#endif
