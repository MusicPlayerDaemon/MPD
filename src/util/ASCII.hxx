// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
[[gnu::pure]] [[gnu::nonnull]]
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

[[gnu::pure]] [[gnu::nonnull]]
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

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringStartsWithCaseASCII(const char *haystack,
			  std::string_view needle) noexcept
{
	return StringEqualsCaseASCII(haystack, needle.data(), needle.length());
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringAfterPrefixCaseASCII(const char *haystack,
			   std::string_view needle) noexcept
{
	return StringStartsWithCaseASCII(haystack, needle)
		? haystack + needle.length()
		: nullptr;
}

#endif
