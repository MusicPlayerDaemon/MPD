/*
 * Copyright 2008-2019 Max Kellermann <max.kellermann@gmail.com>
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

#include "UriExtract.hxx"
#include "CharUtil.hxx"
#include "StringView.hxx"

#include <string.h>

static constexpr bool
IsValidSchemeStart(char ch)
{
	return IsLowerAlphaASCII(ch);
}

static constexpr bool
IsValidSchemeChar(char ch)
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch) ||
		ch == '+' || ch == '.' || ch == '-';
}

gcc_pure
static bool
IsValidScheme(std::string_view p) noexcept
{
	if (p.empty() || !IsValidSchemeStart(p.front()))
		return false;

	for (size_t i = 1; i < p.size(); ++i)
		if (!IsValidSchemeChar(p[i]))
			return false;

	return true;
}

/**
 * Return the URI part after the scheme specification (and after the
 * double slash).
 */
gcc_pure
static std::string_view
uri_after_scheme(std::string_view uri) noexcept
{
	if (uri.length() > 2 &&
	    uri[0] == '/' && uri[1] == '/' && uri[2] != '/')
		return uri.substr(2);

	auto colon = uri.find(':');
	if (colon == std::string_view::npos ||
	    !IsValidScheme(uri.substr(0, colon)))
		return {};

	uri = uri.substr(colon + 1);
	if (uri[0] != '/' || uri[1] != '/')
		return {};

	return uri.substr(2);
}

bool
uri_has_scheme(const char *uri) noexcept
{
	return !uri_get_scheme(uri).empty();
}

std::string_view
uri_get_scheme(std::string_view uri) noexcept
{
	auto end = uri.find("://");
	if (end == std::string_view::npos)
		return {};

	return uri.substr(0, end);
}

bool
uri_is_relative_path(const char *uri) noexcept
{
	return !uri_has_scheme(uri) && *uri != '/';
}

std::string_view
uri_get_path(std::string_view uri) noexcept
{
	auto ap = uri_after_scheme(uri);
	if (ap.data() != nullptr) {
		auto slash = ap.find('/');
		if (slash == std::string_view::npos)
			return {};
		return ap.substr(slash);
	}

	return uri;
}

/* suffixes should be ascii only characters */
const char *
uri_get_suffix(const char *uri) noexcept
{
	const char *suffix = std::strrchr(uri, '.');
	if (suffix == nullptr || suffix == uri ||
	    suffix[-1] == '/' || suffix[-1] == '\\')
		return nullptr;

	++suffix;

	if (strpbrk(suffix, "/\\") != nullptr)
		return nullptr;

	return suffix;
}

const char *
uri_get_suffix(const char *uri, UriSuffixBuffer &buffer) noexcept
{
	const char *suffix = uri_get_suffix(uri);
	if (suffix == nullptr)
		return nullptr;

	const char *q = std::strchr(suffix, '?');
	if (q != nullptr && size_t(q - suffix) < sizeof(buffer.data)) {
		memcpy(buffer.data, suffix, q - suffix);
		buffer.data[q - suffix] = 0;
		suffix = buffer.data;
	}

	return suffix;
}

const char *
uri_get_fragment(const char *uri) noexcept
{
	const char *fragment = std::strchr(uri, '#');
	if (fragment == nullptr)
		return nullptr;

	return fragment + 1;
}
