// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Reader.hxx"
#include "InputStream.hxx"

size_t
InputStreamReader::Read(void *data, size_t size)
{
	size_t nbytes = is.LockRead(data, size);
	assert(nbytes > 0 || is.IsEOF());

	return nbytes;
}
