/*
 * Copyright (C) 2013-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ASCII_HXX
#define ASCII_HXX

#include "Compiler.h"

#include <cassert>
#include <string_view>

#include <strings.h>

/**
 * Determine whether two strings are equal, ignoring case for ASCII
 * letters.
 */
gcc_pure gcc_nonnull_all
static inline bool
StringEqualsCaseASCII(const char *a, const char *b) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(a != nullptr);
	assert(b != nullptr);
#endif

	/* note: strcasecmp() depends on the locale, but for ASCII-only
	   strings, it's safe to use */
	return strcasecmp(a, b) == 0;
}

gcc_pure gcc_nonnull_all
static inline bool
StringEqualsCaseASCII(const char *a, const char *b, size_t n) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(a != nullptr);
	assert(b != nullptr);
#endif

	/* note: strcasecmp() depends on the locale, but for ASCII-only
	   strings, it's safe to use */
	return strncasecmp(a, b, n) == 0;
}

gcc_pure gcc_nonnull_all
static inline bool
StringStartsWithCaseASCII(const char *haystack,
			  std::string_view needle) noexcept
{
	return StringEqualsCaseASCII(haystack, needle.data(), needle.length());
}

gcc_pure gcc_nonnull_all
static inline const char *
StringAfterPrefixCaseASCII(const char *haystack,
			   std::string_view needle) noexcept
{
	return StringStartsWithCaseASCII(haystack, needle)
		? haystack + needle.length()
		: nullptr;
}

#endif
