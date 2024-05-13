// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Reader.hxx"
#include "InputStream.hxx"

std::size_t
InputStreamReader::Read(std::span<std::byte> dest)
{
	size_t nbytes = is.LockRead(dest);
	assert(nbytes > 0 || is.IsEOF());

	return nbytes;
}
