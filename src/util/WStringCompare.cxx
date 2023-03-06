// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "WStringCompare.hxx"

#include <string.h>

bool
StringEndsWith(const wchar_t *haystack, const wchar_t *needle) noexcept
{
	const size_t haystack_length = StringLength(haystack);
	const size_t needle_length = StringLength(needle);

	return haystack_length >= needle_length &&
		StringIsEqual(haystack + haystack_length - needle_length, needle);
}

bool
StringEndsWithIgnoreCase(const wchar_t *haystack,
			 const wchar_t *needle) noexcept
{
	const size_t haystack_length = StringLength(haystack);
	const size_t needle_length = StringLength(needle);

	return haystack_length >= needle_length &&
		StringIsEqualIgnoreCase(haystack + haystack_length - needle_length,
					needle);
}

const wchar_t *
FindStringSuffix(const wchar_t *p, const wchar_t *suffix) noexcept
{
	const size_t p_length = StringLength(p);
	const size_t suffix_length = StringLength(suffix);

	if (p_length < suffix_length)
		return nullptr;

	const auto *q = p + p_length - suffix_length;
	return memcmp(q, suffix, suffix_length * sizeof(*suffix)) == 0
		? q
		: nullptr;
}
