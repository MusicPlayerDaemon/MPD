// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Escape.hxx"
#include "Easy.hxx"
#include "String.hxx"
#include "util/IterableSplitString.hxx"

std::string
CurlEscapeUriPath(CURL *curl, std::string_view src) noexcept
{
	std::string dest;

	for (const auto i : IterableSplitString(src, '/')) {
		CurlString escaped(curl_easy_escape(curl, i.data(), i.size()));
		if (!dest.empty())
			dest.push_back('/');
		dest += escaped.c_str();
	}

	return dest;
}

std::string
CurlEscapeUriPath(std::string_view src) noexcept
{
	CurlEasy easy;
	return CurlEscapeUriPath(easy.Get(), src);
}

std::string
CurlUnescape(CURL *curl, std::string_view src) noexcept
{
	int outlength;
	CurlString tmp(curl_easy_unescape(curl, src.data(), src.size(),
					  &outlength));
	return {tmp.c_str(), size_t(outlength)};
}

std::string
CurlUnescape(std::string_view src) noexcept
{
	CurlEasy easy;
	return CurlUnescape(easy.Get(), src);
}
