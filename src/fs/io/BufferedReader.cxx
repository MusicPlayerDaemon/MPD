/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "BufferedReader.hxx"
#include "Reader.hxx"
#include "util/TextFile.hxx"

bool
BufferedReader::Fill(bool need_more)
{
	if (gcc_unlikely(last_error.IsDefined()))
		return false;

	if (eof)
		return !need_more;

	auto w = buffer.Write();
	if (w.IsEmpty()) {
		if (buffer.GetCapacity() >= MAX_SIZE)
			return !need_more;

		buffer.Grow(buffer.GetCapacity() * 2);
		w = buffer.Write();
		assert(!w.IsEmpty());
	}

	size_t nbytes = reader.Read(w.data, w.size, last_error);
	if (nbytes == 0) {
		if (gcc_unlikely(last_error.IsDefined()))
			return false;

		eof = true;
		return !need_more;
	}

	buffer.Append(nbytes);
	return true;
}

char *
BufferedReader::ReadLine()
{
	do {
		char *line = ReadBufferedLine(buffer);
		if (line != nullptr)
			return line;
	} while (Fill(true));

	if (last_error.IsDefined() || !eof || buffer.IsEmpty())
		return nullptr;

	auto w = buffer.Write();
	if (w.IsEmpty()) {
		buffer.Grow(buffer.GetCapacity() + 1);
		w = buffer.Write();
		assert(!w.IsEmpty());
	}

	/* terminate the last line */
	w[0] = 0;

	char *line = buffer.Read().data;
	buffer.Clear();
	return line;
}
