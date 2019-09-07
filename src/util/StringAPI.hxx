/*
 * Copyright 2010-2019 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_API_HXX
#define STRING_API_HXX

#include "Compiler.h"

#include <string.h>

#ifdef _UNICODE
#include "WStringAPI.hxx"
#endif

gcc_pure gcc_nonnull_all
static inline size_t
StringLength(const char *p) noexcept
{
	return strlen(p);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFind(const char *haystack, const char *needle) noexcept
{
	return strstr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline char *
StringFind(char *haystack, char needle, size_t size) noexcept
{
	return (char *)memchr(haystack, needle, size);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFind(const char *haystack, char needle, size_t size) noexcept
{
	return (const char *)memchr(haystack, needle, size);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFind(const char *haystack, char needle) noexcept
{
	return strchr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline char *
StringFind(char *haystack, char needle) noexcept
{
	return strchr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFindLast(const char *haystack, char needle) noexcept
{
	return strrchr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline char *
StringFindLast(char *haystack, char needle) noexcept
{
	return strrchr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFindLast(const char *haystack, char needle, size_t size) noexcept
{
#if defined(__GLIBC__) || defined(__BIONIC__)
	/* memrchr() is a GNU extension (and also available on
	   Android) */
	return (const char *)memrchr(haystack, needle, size);
#else
	/* emulate for everybody else */
	const auto *p = haystack + size;
	while (p > haystack) {
		--p;
		if (*p == needle)
			return p;
	}

	return nullptr;
#endif
}

gcc_pure gcc_nonnull_all
static inline const char *
StringFindAny(const char *haystack, const char *accept) noexcept
{
	return strpbrk(haystack, accept);
}

static inline char *
StringToken(char *str, const char *delim) noexcept
{
	return strtok(str, delim);
}

gcc_nonnull_all
static inline void
UnsafeCopyString(char *dest, const char *src) noexcept
{
	strcpy(dest, src);
}

gcc_returns_nonnull gcc_nonnull_all
static inline char *
UnsafeCopyStringP(char *dest, const char *src) noexcept
{
#if defined(_WIN32)
	/* emulate stpcpy() */
	UnsafeCopyString(dest, src);
	return dest + StringLength(dest);
#else
	return stpcpy(dest, src);
#endif
}

gcc_pure gcc_nonnull_all
static inline int
StringCompare(const char *a, const char *b) noexcept
{
	return strcmp(a, b);
}

gcc_pure gcc_nonnull_all
static inline int
StringCompare(const char *a, const char *b, size_t n) noexcept
{
	return strncmp(a, b, n);
}

/**
 * Checks whether #a and #b are equal.
 */
gcc_pure gcc_nonnull_all
static inline bool
StringIsEqual(const char *a, const char *b) noexcept
{
	return StringCompare(a, b) == 0;
}

/**
 * Checks whether #a and #b are equal.
 */
gcc_pure gcc_nonnull_all
static inline bool
StringIsEqual(const char *a, const char *b, size_t length) noexcept
{
	return strncmp(a, b, length) == 0;
}

gcc_pure gcc_nonnull_all
static inline bool
StringIsEqualIgnoreCase(const char *a, const char *b) noexcept
{
	return strcasecmp(a, b) == 0;
}

gcc_pure gcc_nonnull_all
static inline bool
StringIsEqualIgnoreCase(const char *a, const char *b, size_t size) noexcept
{
	return strncasecmp(a, b, size) == 0;
}

gcc_pure gcc_nonnull_all
static inline int
StringCollate(const char *a, const char *b) noexcept
{
	return strcoll(a, b);
}

/**
 * Copy the string to a new allocation.  The return value must be
 * freed with free().
 */
gcc_malloc gcc_returns_nonnull gcc_nonnull_all
static inline char *
DuplicateString(const char *p) noexcept
{
	return strdup(p);
}

#endif
