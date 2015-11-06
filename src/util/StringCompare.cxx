/*
 * Copyright (C) 2013-2015 Max Kellermann <max@duempel.org>
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
#include "StringAPI.hxx"

#include <assert.h>
#include <string.h>

bool
StringStartsWith(const char *haystack, const char *needle)
{
	const size_t length = StringLength(needle);
	return StringIsEqual(haystack, needle, length);
}

bool
StringEndsWith(const char *haystack, const char *needle)
{
	const size_t haystack_length = strlen(haystack);
	const size_t needle_length = strlen(needle);

	return haystack_length >= needle_length &&
		memcmp(haystack + haystack_length - needle_length,
		       needle, needle_length) == 0;
}

const char *
StringAfterPrefix(const char *string, const char *prefix)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(string != nullptr);
	assert(prefix != nullptr);
#endif

	size_t prefix_length = strlen(prefix);
	return StringIsEqual(string, prefix, prefix_length)
		? string + prefix_length
		: nullptr;
}

const char *
FindStringSuffix(const char *p, const char *suffix)
{
	const size_t p_length = strlen(p);
	const size_t suffix_length = strlen(suffix);

	if (p_length < suffix_length)
		return nullptr;

	const char *q = p + p_length - suffix_length;
	return memcmp(q, suffix, suffix_length) == 0
		? q
		: nullptr;
}
