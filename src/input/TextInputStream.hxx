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

#ifndef MPD_TEXT_INPUT_STREAM_HXX
#define MPD_TEXT_INPUT_STREAM_HXX

#include "input/Ptr.hxx"
#include "util/StaticFifoBuffer.hxx"

class TextInputStream {
	InputStreamPtr is;
	StaticFifoBuffer<char, 4096> buffer;

public:
	/**
	 * Wraps an existing #InputStream object into a #TextInputStream,
	 * to read its contents as text lines.
	 *
	 * @param _is an open #InputStream object
	 */
	explicit TextInputStream(InputStreamPtr &&_is) noexcept;
	~TextInputStream() noexcept;

	TextInputStream(const TextInputStream &) = delete;
	TextInputStream& operator=(const TextInputStream &) = delete;

	InputStreamPtr &&StealInputStream() noexcept {
		return std::move(is);
	}

	/**
	 * Reads the next line from the stream with newline character stripped.
	 *
	 * Throws on error.
	 *
	 * @return a pointer to the line, or nullptr on end-of-file
	 */
	char *ReadLine();
};

#endif
