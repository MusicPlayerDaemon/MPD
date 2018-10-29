/*
 * Copyright 2013-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_COMPARE_HXX
#define STRING_COMPARE_HXX

#include "StringView.hxx"
#include "StringAPI.hxx"
#include "Compiler.h"

#ifdef _UNICODE
#include "WStringCompare.hxx"
#endif

gcc_pure gcc_nonnull_all
static inline bool
StringIsEmpty(const char *string) noexcept
{
	return *string == 0;
}

gcc_pure gcc_nonnull_all
static inline bool
StringStartsWith(const char *haystack, StringView needle) noexcept
{
	return StringIsEqual(haystack, needle.data, needle.size);
}

gcc_pure gcc_nonnull_all
bool
StringEndsWith(const char *haystack, const char *needle) noexcept;

gcc_pure gcc_nonnull_all
bool
StringEndsWithIgnoreCase(const char *haystack, const char *needle) noexcept;

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 */
gcc_pure gcc_nonnull_all
static inline const char *
StringAfterPrefix(const char *haystack, StringView needle) noexcept
{
	return StringStartsWith(haystack, needle)
		? haystack + needle.size
		: nullptr;
}

gcc_pure gcc_nonnull_all
static inline bool
StringStartsWithIgnoreCase(const char *haystack, StringView needle) noexcept
{
	return StringIsEqualIgnoreCase(haystack, needle.data, needle.size);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringAfterPrefixIgnoreCase(const char *haystack, StringView needle) noexcept
{
	return StringStartsWithIgnoreCase(haystack, needle)
		? haystack + needle.size
		: nullptr;
}

/**
 * Check if the given string ends with the specified suffix.  If yes,
 * returns the position of the suffix, and nullptr otherwise.
 */
gcc_pure gcc_nonnull_all
const char *
FindStringSuffix(const char *p, const char *suffix) noexcept;

#endif
