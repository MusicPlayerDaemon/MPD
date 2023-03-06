// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ParseInputStream.hxx"
#include "Handle.hxx"
#include "input/InputStream.hxx"

void
Yajl::ParseInputStream(Handle &handle, InputStream &is)
{
	while (true) {
		unsigned char buffer[4096];
		const size_t nbytes = is.LockRead(buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		handle.Parse(buffer, nbytes);
	}

	handle.CompleteParse();
}
