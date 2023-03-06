// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ExpatParser.hxx"
#include "input/InputStream.hxx"

void
ExpatParser::Parse(InputStream &is)
{
	assert(is.IsReady());

	while (true) {
		char buffer[4096];
		size_t nbytes = is.LockRead(buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		Parse(buffer, nbytes);
	}

	CompleteParse();
}
