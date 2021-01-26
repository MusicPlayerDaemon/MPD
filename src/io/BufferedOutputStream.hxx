/*
 * Copyright 2014-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BUFFERED_OUTPUT_STREAM_HXX
#define BUFFERED_OUTPUT_STREAM_HXX

#include "util/Compiler.h"
#include "util/DynamicFifoBuffer.hxx"

#include <cstddef>

#ifdef _UNICODE
#include <wchar.h>
#endif

class OutputStream;

/**
 * An #OutputStream wrapper that buffers its output to reduce the
 * number of OutputStream::Write() calls.
 *
 * All wchar_t based strings are converted to UTF-8.
 *
 * To make sure everything is written to the underlying #OutputStream,
 * call Flush() before destructing this object.
 */
class BufferedOutputStream {
	OutputStream &os;

	DynamicFifoBuffer<std::byte> buffer;

public:
	explicit BufferedOutputStream(OutputStream &_os,
				      size_t buffer_size=32768) noexcept
		:os(_os), buffer(buffer_size) {}

	/**
	 * Write the contents of a buffer.
	 */
	void Write(const void *data, std::size_t size);

	/**
	 * Write the given object.  Note that this is only safe with
	 * POD types.  Types with padding can expose sensitive data.
	 */
	template<typename T>
	void WriteT(const T &value) {
		Write(&value, sizeof(value));
	}

	/**
	 * Write one narrow character.
	 */
	void Write(const char &ch) {
		WriteT(ch);
	}

	/**
	 * Write a null-terminated string.
	 */
	void Write(const char *p);

	/**
	 * Write a printf-style formatted string.
	 */
	gcc_printf(2,3)
	void Format(const char *fmt, ...);

#ifdef _UNICODE
	/**
	 * Write one narrow character.
	 */
	void Write(const wchar_t &ch) {
		WriteWideToUTF8(&ch, 1);
	}

	/**
	 * Write a null-terminated wide string.
	 */
	void Write(const wchar_t *p);
#endif

	/**
	 * Write buffer contents to the #OutputStream.
	 */
	void Flush();

	/**
	 * Discard buffer contents.
	 */
	void Discard() noexcept {
		buffer.Clear();
	}

private:
	bool AppendToBuffer(const void *data, std::size_t size) noexcept;

#ifdef _UNICODE
	void WriteWideToUTF8(const wchar_t *p, std::size_t length);
#endif
};

/**
 * Helper function which constructs a #BufferedOutputStream, calls the
 * given function and flushes the #BufferedOutputStream.
 */
template<typename F>
void
WithBufferedOutputStream(OutputStream &os, F &&f)
{
	BufferedOutputStream bos(os);
	f(bos);
	bos.Flush();
}

#endif
