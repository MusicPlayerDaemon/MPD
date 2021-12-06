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
		fd.FullRead(buffer + nbytes, rest);
		nbytes += rest;
	}

	return nbytes;
}
