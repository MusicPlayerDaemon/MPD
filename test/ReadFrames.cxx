// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ReadFrames.hxx"
#include "system/Error.hxx"
#include "io/FileDescriptor.hxx"

static size_t
ReadOrThrow(FileDescriptor fd, void *buffer, size_t size)
{
	auto nbytes = fd.Read(buffer, size);
	if (nbytes < 0)
		throw MakeErrno("Read failed");

	return nbytes;
}

std::size_t
ReadFrames(FileDescriptor fd, void *_buffer, std::size_t size,
	   std::size_t frame_size)
{
	auto buffer = (std::byte *)_buffer;

	size = (size / frame_size) * frame_size;

	size_t nbytes = ReadOrThrow(fd, buffer, size);

	const size_t modulo = nbytes % frame_size;
	if (modulo > 0) {
		size_t rest = frame_size - modulo;
		fd.FullRead({(std::byte *)buffer + nbytes, rest});
		nbytes += rest;
	}

	return nbytes;
}
