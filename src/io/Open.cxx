/*
 * Copyright 2018 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/Error.hxx"

#include <fcntl.h>

UniqueFileDescriptor
OpenReadOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_RDONLY|flags))
		throw FormatErrno("Failed to open '%s'", path);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_WRONLY|flags))
		throw FormatErrno("Failed to open '%s'", path);

	return fd;
}

#ifndef _WIN32

UniqueFileDescriptor
OpenDirectory(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_DIRECTORY|O_RDONLY|flags))
		throw FormatErrno("Failed to open '%s'", path);

	return fd;
}

#endif

#ifdef __linux__

UniqueFileDescriptor
OpenPath(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_PATH|flags))
		throw FormatErrno("Failed to open '%s'", path);

	return fd;
}

UniqueFileDescriptor
OpenPath(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_PATH|flags))
		throw FormatErrno("Failed to open '%s'", name);

	return fd;
}

UniqueFileDescriptor
OpenReadOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_RDONLY|flags))
		throw FormatErrno("Failed to open '%s'", name);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_WRONLY|flags))
		throw FormatErrno("Failed to open '%s'", name);

	return fd;
}

UniqueFileDescriptor
OpenDirectory(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_DIRECTORY|O_RDONLY|flags))
		throw FormatErrno("Failed to open '%s'", name);

	return fd;
}

#endif
