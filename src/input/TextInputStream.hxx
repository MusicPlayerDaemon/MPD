// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
