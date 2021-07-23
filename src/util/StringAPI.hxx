/*
 * Copyright 2010-2021 Max Kellermann <max.kellermann@gmail.com>
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

#include <cstring>

#ifdef _UNICODE
#include "WStringAPI.hxx"
#endif

[[gnu::pure]] [[gnu::nonnull]]
static inline size_t
StringLength(const char *p) noexcept
{
	return strlen(p);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringFind(const char *haystack, const char *needle) noexcept
{
	return strstr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline char *
StringFind(char *haystack, char needle, size_t size) noexcept
{
	return (char *)std::memchr(haystack, needle, size);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringFind(const char *haystack, char needle, size_t size) noexcept
{
	return (const char *)std::memchr(haystack, needle, size);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringFind(const char *haystack, char needle) noexcept
{
	return std::strchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline char *
StringFind(char *haystack, char needle) noexcept
{
	return std::strchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const char *
StringFindLast(const char *haystack, char needle) noexcept
{
	return std::strrchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline char *
StringFindLast(char *haystack, char needle) noexcept
{
	return std::strrchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
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

[[gnu::pure]] [[gnu::nonnull]]
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

[[gnu::nonnull]]
static inline void
UnsafeCopyString(char *dest, const char *src) noexcept
{
	strcpy(dest, src);
}

[[gnu::returns_nonnull]] [[gnu::nonnull]]
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

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCompare(const char *a, const char *b) noexcept
{
	return strcmp(a, b);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCompare(const char *a, const char *b, size_t n) noexcept
{
	return strncmp(a, b, n);
}

/**
 * Checks whether #a and #b are equal.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqual(const char *a, const char *b) noexcept
{
	return StringCompare(a, b) == 0;
}

/**
 * Checks whether #a and #b are equal.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqual(const char *a, const char *b, size_t length) noexcept
{
	return strncmp(a, b, length) == 0;
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqualIgnoreCase(const char *a, const char *b) noexcept
{
#ifdef _MSC_VER
	return _stricmp(a, b) == 0;
#else
	return strcasecmp(a, b) == 0;
#endif
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqualIgnoreCase(const char *a, const char *b, size_t size) noexcept
{
#ifdef _MSC_VER
	return _strnicmp(a, b, size) == 0;
#else
	return strncasecmp(a, b, size) == 0;
#endif
}

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCollate(const char *a, const char *b) noexcept
{
	return strcoll(a, b);
}

/**
 * Copy the string to a new allocation.  The return value must be
 * freed with free().
 */
[[gnu::malloc]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
static inline char *
DuplicateString(const char *p) noexcept
{
	return strdup(p);
}

#endif
