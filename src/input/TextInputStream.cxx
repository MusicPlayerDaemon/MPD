// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TextInputStream.hxx"
#include "InputStream.hxx"
#include "util/TextFile.hxx"

#include <cassert>

TextInputStream::TextInputStream(InputStreamPtr &&_is) noexcept
	:is(std::move(_is)) {}

TextInputStream::~TextInputStream() noexcept = default;

char *
TextInputStream::ReadLine()
{
	char *line = ReadBufferedLine(buffer);
	if (line != nullptr)
		return line;

	buffer.Shift();

	while (true) {
		auto dest = buffer.Write();
		if (dest.size() < 2) {
			/* line too long: terminate the current
			   line */

			assert(!dest.empty());
			dest[0] = 0;
			line = buffer.Read().data();
			buffer.Clear();
			return line;
		}

		/* reserve one byte for the null terminator if the
		   last line is not terminated by a newline
		   character */
		dest = dest.first(dest.size() - 1);

		size_t nbytes = is->LockRead(std::as_writable_bytes(dest));

		buffer.Append(nbytes);

		line = ReadBufferedLine(buffer);
		if (line != nullptr)
			return line;

		if (nbytes == 0) {
			/* end of file: see if there's an unterminated
			   line */

			dest = buffer.Write();
			assert(!dest.empty());
			dest[0] = 0;

			auto r = buffer.Read();
			buffer.Clear();
			return r.empty()
				? nullptr
				: r.data();
		}
	}
}
