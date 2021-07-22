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

#ifndef WSTRING_API_HXX
#define WSTRING_API_HXX

#include <cwchar>

[[gnu::pure]] [[gnu::nonnull]]
static inline size_t
StringLength(const wchar_t *p) noexcept
{
	return wcslen(p);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFind(const wchar_t *haystack, const wchar_t *needle) noexcept
{
	return wcsstr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFind(const wchar_t *haystack, wchar_t needle, size_t size) noexcept
{
	return std::wmemchr(haystack, needle, size);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline wchar_t *
StringFind(wchar_t *haystack, wchar_t needle, size_t size) noexcept
{
	return std::wmemchr(haystack, needle, size);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFind(const wchar_t *haystack, wchar_t needle) noexcept
{
	return wcschr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline wchar_t *
StringFind(wchar_t *haystack, wchar_t needle) noexcept
{
	return wcschr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFindLast(const wchar_t *haystack, wchar_t needle) noexcept
{
	return wcsrchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline wchar_t *
StringFindLast(wchar_t *haystack, wchar_t needle) noexcept
{
	return wcsrchr(haystack, needle);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFindLast(const wchar_t *haystack, wchar_t needle, size_t size) noexcept
{
	/* there's no wmemrchr() unfortunately */
	const auto *p = haystack + size;
	while (p > haystack) {
		--p;
		if (*p == needle)
			return p;
	}

	return nullptr;
}

[[gnu::pure]] [[gnu::nonnull]]
static inline const wchar_t *
StringFindAny(const wchar_t *haystack, const wchar_t *accept) noexcept
{
	return wcspbrk(haystack, accept);
}

[[gnu::nonnull]]
static inline void
UnsafeCopyString(wchar_t *dest, const wchar_t *src) noexcept
{
	wcscpy(dest, src);
}

[[gnu::returns_nonnull]] [[gnu::nonnull]]
static inline wchar_t *
UnsafeCopyStringP(wchar_t *dest, const wchar_t *src) noexcept
{
#if defined(_WIN32) || defined(__OpenBSD__) || defined(__NetBSD__)
	/* emulate wcpcpy() */
	UnsafeCopyString(dest, src);
	return dest + StringLength(dest);
#elif defined(__sun) && defined (__SVR4)
	return std::wcpcpy(dest, src);
#else
	return wcpcpy(dest, src);
#endif
}

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCompare(const wchar_t *a, const wchar_t *b) noexcept
{
	return wcscmp(a, b);
}

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCompare(const wchar_t *a, const wchar_t *b, size_t n) noexcept
{
	return wcsncmp(a, b, n);
}

/**
 * Checks whether str1 and str2 are equal.
 * @param str1 String 1
 * @param str2 String 2
 * @return True if equal, False otherwise
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqual(const wchar_t *str1, const wchar_t *str2) noexcept
{
	return StringCompare(str1, str2) == 0;
}

/**
 * Checks whether #a and #b are equal.
 */
[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqual(const wchar_t *a, const wchar_t *b, size_t length) noexcept
{
	return wcsncmp(a, b, length) == 0;
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqualIgnoreCase(const wchar_t *a, const wchar_t *b) noexcept
{
#ifdef _WIN32
	return _wcsicmp(a, b) == 0;
#else
	return wcscasecmp(a, b) == 0;
#endif
}

[[gnu::pure]] [[gnu::nonnull]]
static inline bool
StringIsEqualIgnoreCase(const wchar_t *a, const wchar_t *b,
			size_t size) noexcept
{
#ifdef _WIN32
	return _wcsnicmp(a, b, size) == 0;
#else
	return wcsncasecmp(a, b, size) == 0;
#endif
}

[[gnu::pure]] [[gnu::nonnull]]
static inline int
StringCollate(const wchar_t *a, const wchar_t *b) noexcept
{
	return wcscoll(a, b);
}

[[gnu::malloc]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
static inline wchar_t *
DuplicateString(const wchar_t *p) noexcept
{
#if defined(__sun) && defined (__SVR4)
	return std::wcsdup(p);
#else
	return wcsdup(p);
#endif
}

#endif
