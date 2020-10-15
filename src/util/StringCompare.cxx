/*
 * Copyright 2013-2020 Max Kellermann <max.kellermann@gmail.com>
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
