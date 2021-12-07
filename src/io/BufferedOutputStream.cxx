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

#include "BufferedOutputStream.hxx"
#include "OutputStream.hxx"

#include <cstdarg>

#include <string.h>
#include <stdio.h>

#ifdef _UNICODE
#include "system/Error.hxx"
#include <stringapiset.h>
#endif

bool
BufferedOutputStream::AppendToBuffer(const void *data, std::size_t size) noexcept
{
	auto r = buffer.Write();
	if (r.size < size)
		return false;

	memcpy(r.data, data, size);
	buffer.Append(size);
	return true;
}

void
BufferedOutputStream::Write(const void *data, std::size_t size)
{
	/* try to append to the current buffer */
	if (AppendToBuffer(data, size))
		return;

	/* not enough room in the buffer - flush it */
	Flush();

	/* see if there's now enough room */
	if (AppendToBuffer(data, size))
		return;

	/* too large for the buffer: direct write */
	os.Write(data, size);
}

void
BufferedOutputStream::Write(const char *p)
{
	Write(p, strlen(p));
}

void
BufferedOutputStream::Format(const char *fmt, ...)
{
	auto r = buffer.Write();
	if (r.empty()) {
		Flush();
		r = buffer.Write();
	}

	/* format into the buffer */
	std::va_list ap;
	va_start(ap, fmt);
	std::size_t size = vsnprintf((char *)r.data, r.size, fmt, ap);
	va_end(ap);

	if (gcc_unlikely(size >= r.size)) {
		/* buffer was not large enough; flush it and try
		   again */

		Flush();

		r = buffer.Write();

		if (gcc_unlikely(size >= r.size)) {
			/* still not enough space: grow the buffer and
			   try again */
			r.size = size + 1;
			r.data = buffer.Write(r.size);
		}

		/* format into the new buffer */
		va_start(ap, fmt);
		size = vsnprintf((char *)r.data, r.size, fmt, ap);
		va_end(ap);

		/* this time, it must fit */
		assert(size < r.size);
	}

	buffer.Append(size);
}

#ifdef _UNICODE

void
BufferedOutputStream::Write(const wchar_t *p)
{
	WriteWideToUTF8(p, wcslen(p));
}

void
BufferedOutputStream::WriteWideToUTF8(const wchar_t *src,
				      std::size_t src_length)
{
	if (src_length == 0)
		return;

	auto r = buffer.Write();
	if (r.empty()) {
		Flush();
		r = buffer.Write();
	}

	int length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					 (char *)r.data, r.size,
					 nullptr, nullptr);
	if (length <= 0) {
		const auto error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* how much buffer do we need? */
		length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					     nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* grow the buffer and try again */
		length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					     (char *)buffer.Write(length), length,
					     nullptr, nullptr);
		if (length <= 0)
			throw MakeLastError(error, "UTF-8 conversion failed");
	}

	buffer.Append(length);
}

#endif

void
BufferedOutputStream::Flush()
{
	auto r = buffer.Read();
	if (r.empty())
		return;

	os.Write(r.data, r.size);
	buffer.Consume(r.size);
}
