// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BufferedOutputStream.hxx"
#include "OutputStream.hxx"
#include "util/SpanCast.hxx"

#include <fmt/format.h>

#include <cstdarg>
#include <iterator> // for std:back_inserter()

#include <string.h>

#ifdef _UNICODE
#include "system/Error.hxx"
#include <stringapiset.h>
#endif

bool
BufferedOutputStream::AppendToBuffer(std::span<const std::byte> src) noexcept
{
	auto w = buffer.Write();
	if (w.size() < src.size())
		return false;

	std::copy(src.begin(), src.end(), w.begin());
	buffer.Append(src.size());
	return true;
}

void
BufferedOutputStream::Write(std::span<const std::byte> src)
{
	/* try to append to the current buffer */
	if (AppendToBuffer(src))
		return;

	/* not enough room in the buffer - flush it */
	Flush();

	/* see if there's now enough room */
	if (AppendToBuffer(src))
		return;

	/* too large for the buffer: direct write */
	os.Write(src);
}

void
BufferedOutputStream::VFmt(fmt::string_view format_str, fmt::format_args args)
{
	/* TODO format into this object's buffer instead of allocating
	   a new one */
	fmt::memory_buffer b;
	fmt::vformat_to(std::back_inserter(b), format_str, args);
	return Write(std::as_bytes(std::span{b.data(), b.size()}));
}

#ifdef _UNICODE

void
BufferedOutputStream::WriteWideToUTF8(std::wstring_view src)
{
	if (src.empty())
		return;

	auto r = buffer.Write();
	if (r.empty()) {
		Flush();
		r = buffer.Write();
	}

	int length = WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(),
					 (char *)r.data(), r.size(),
					 nullptr, nullptr);
	if (length <= 0) {
		const auto error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* how much buffer do we need? */
		length = WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(),
					     nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* grow the buffer and try again */
		length = WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(),
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

	os.Write(r);
	buffer.Consume(r.size());
}
