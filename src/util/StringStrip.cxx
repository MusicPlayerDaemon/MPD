// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StringStrip.hxx"
#include "CharUtil.hxx"

#include <algorithm>
#include <cstring>

const char *
StripLeft(const char *p) noexcept
{
	while (IsWhitespaceNotNull(*p))
		++p;

	return p;
}

const char *
StripLeft(const char *p, const char *end) noexcept
{
	while (p < end && IsWhitespaceOrNull(*p))
		++p;

	return p;
}

std::string_view
StripLeft(const std::string_view s) noexcept
{
	auto i = std::find_if_not(s.begin(), s.end(),
				  [](auto ch){ return IsWhitespaceOrNull(ch); });

#ifdef __clang__
	// libc++ doesn't yet support the C++20 constructor
	return s.substr(std::distance(s.begin(), i));
#else
	return {
		i,
		s.end(),
	};
#endif
}

const char *
StripRight(const char *p, const char *end) noexcept
{
	while (end > p && IsWhitespaceOrNull(end[-1]))
		--end;

	return end;
}

std::size_t
StripRight(const char *p, std::size_t length) noexcept
{
	while (length > 0 && IsWhitespaceOrNull(p[length - 1]))
		--length;

	return length;
}

void
StripRight(char *p) noexcept
{
	std::size_t old_length = std::strlen(p);
	std::size_t new_length = StripRight(p, old_length);
	p[new_length] = 0;
}

std::string_view
StripRight(std::string_view s) noexcept
{
	auto i = std::find_if_not(s.rbegin(), s.rend(),
				  [](auto ch){ return IsWhitespaceOrNull(ch); });

	return s.substr(0, std::distance(i, s.rend()));
}

char *
Strip(char *p) noexcept
{
	p = StripLeft(p);
	StripRight(p);
	return p;
}

std::string_view
Strip(std::string_view s) noexcept
{
	return StripRight(StripLeft(s));
}
