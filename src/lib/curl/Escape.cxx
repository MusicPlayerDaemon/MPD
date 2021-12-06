/*
 * Copyright (C) 2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "Escape.hxx"
#include "Easy.hxx"
#include "String.hxx"
#include "util/IterableSplitString.hxx"

std::string
CurlEscapeUriPath(CURL *curl, StringView src) noexcept
{
	std::string dest;

	for (const auto i : IterableSplitString(src, '/')) {
		CurlString escaped(curl_easy_escape(curl, i.data, i.size));
		if (!dest.empty())
			dest.push_back('/');
		dest += escaped.c_str();
	}

	return dest;
}

std::string
CurlEscapeUriPath(StringView src) noexcept
{
	CurlEasy easy;
	return CurlEscapeUriPath(easy.Get(), src);
}

std::string
CurlUnescape(CURL *curl, StringView src) noexcept
{
	int outlength;
	CurlString tmp(curl_easy_unescape(curl, src.data, src.size,
					  &outlength));
	return {tmp.c_str(), size_t(outlength)};
}

std::string
CurlUnescape(StringView src) noexcept
{
	CurlEasy easy;
	return CurlUnescape(easy.Get(), src);
}
