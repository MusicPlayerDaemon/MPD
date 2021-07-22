/*
 * Copyright 2013-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef WSTRING_COMPARE_HXX
#define WSTRING_COMPARE_HXX

#include "WStringView.hxx"
#include "WStringAPI.hxx"

#include <string_view>

#include <wchar.h>

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEmpty(const wchar_t *string) noexcept
{
	return *string == 0;
}

[[gnu::pure]]
static inline bool
StringIsEqual(std::wstring_view a, std::wstring_view b) noexcept
{
	return a.size() == b.size() &&
		StringIsEqual(a.data(), b.data(), b.size());
}

[[gnu::pure]]
static inline bool
StringIsEqualIgnoreCase(std::wstring_view a, std::wstring_view b) noexcept
{
	return a.size() == b.size() &&
		StringIsEqualIgnoreCase(a.data(), b.data(), b.size());
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringStartsWith(const wchar_t *haystack, WStringView needle) noexcept
{
	return StringIsEqual(haystack, needle.data, needle.size);
}

[[gnu::pure]] [[gnu::nonnull]]
bool
StringEndsWith(const wchar_t *haystack, const wchar_t *needle) noexcept;

[[gnu::pure]] [[gnu::nonnull]]
bool
StringEndsWithIgnoreCase(const wchar_t *haystack,
			 const wchar_t *needle) noexcept;

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringAfterPrefix(const wchar_t *haystack, WStringView needle) noexcept
{
	return StringStartsWith(haystack, needle)
		? haystack + needle.size
		: nullptr;
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringStartsWithIgnoreCase(const wchar_t *haystack,
			   WStringView needle) noexcept
{
	return StringIsEqualIgnoreCase(haystack, needle.data, needle.size);
}

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 * This function is case-independent.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringAfterPrefixIgnoreCase(const wchar_t *haystack, WStringView needle) noexcept
{
	return StringStartsWithIgnoreCase(haystack, needle)
		? haystack + needle.size
		: nullptr;
}

/**
 * Check if the given string ends with the specified suffix.  If yes,
 * returns the position of the suffix, and nullptr otherwise.
 */
[[gnu::pure]] [[gnu::nonnull]]
const wchar_t *
FindStringSuffix(const wchar_t *p, const wchar_t *suffix) noexcept;

#endif
