// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ExpatParser.hxx"
#include "input/InputStream.hxx"
#include "util/SpanCast.hxx"

void
ExpatParser::Parse(InputStream &is)
{
	assert(is.IsReady());

	while (true) {
		std::byte buffer[4096];
		size_t nbytes = is.LockRead(buffer);
		if (nbytes == 0)
			break;

		Parse(ToStringView(std::span{buffer}.first(nbytes)));
	}

	CompleteParse();
}
