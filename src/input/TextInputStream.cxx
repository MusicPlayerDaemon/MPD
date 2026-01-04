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
	if (!bom_checked) {
		bom_checked = true;

		/* try to strip a UTF-8 BOM;
		   keep all bytes if it's not a BOM */
		auto dest = buffer.Write();
		assert(dest.size() >= 3);
		dest = dest.first(3);
		size_t nbytes = is->LockRead(std::as_writable_bytes(dest));
		buffer.Append(nbytes);

		auto r = buffer.Read();
		if (r.size() >= 3 &&
		    static_cast<unsigned char>(r[0]) == 0xEF &&
		    static_cast<unsigned char>(r[1]) == 0xBB &&
		    static_cast<unsigned char>(r[2]) == 0xBF) {
			buffer.Consume(3);
		}
	}

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
