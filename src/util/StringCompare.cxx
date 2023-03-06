// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StringCompare.hxx"

#include <cstring>

bool
StringEndsWith(const char *haystack, const char *needle) noexcept
{
	const size_t haystack_length = StringLength(haystack);
	const size_t needle_length = StringLength(needle);

	return haystack_length >= needle_length &&
		std::memcmp(haystack + haystack_length - needle_length,
			    needle, needle_length) == 0;
}

bool
StringEndsWithIgnoreCase(const char *haystack, const char *needle) noexcept
{
	const size_t haystack_length = StringLength(haystack);
	const size_t needle_length = StringLength(needle);

	return haystack_length >= needle_length &&
		StringIsEqualIgnoreCase(haystack + haystack_length - needle_length,
					needle);
}

const char *
FindStringSuffix(const char *p, const char *suffix) noexcept
{
	const size_t p_length = StringLength(p);
	const size_t suffix_length = StringLength(suffix);

	if (p_length < suffix_length)
		return nullptr;

	const char *q = p + p_length - suffix_length;
	return std::memcmp(q, suffix, suffix_length) == 0
		? q
		: nullptr;
}
