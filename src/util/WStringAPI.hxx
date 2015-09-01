/*
 * Copyright (C) 2010-2015 Max Kellermann <max@duempel.org>
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

#include "Compiler.h"

#include <wchar.h>

gcc_pure gcc_nonnull_all
static inline size_t
StringLength(const wchar_t *p)
{
	return wcslen(p);
}

gcc_pure gcc_nonnull_all
static inline const wchar_t *
StringFind(const wchar_t *haystack, const wchar_t *needle)
{
	return wcsstr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline const wchar_t *
StringFind(const wchar_t *haystack, wchar_t needle, size_t size)
{
	return wmemchr(haystack, needle, size);
}

gcc_pure gcc_nonnull_all
static inline wchar_t *
StringFind(wchar_t *haystack, wchar_t needle, size_t size)
{
	return wmemchr(haystack, needle, size);
}

gcc_pure gcc_nonnull_all
static inline const wchar_t *
StringFind(const wchar_t *haystack, wchar_t needle)
{
	return wcschr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline wchar_t *
StringFind(wchar_t *haystack, wchar_t needle)
{
	return wcschr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline const wchar_t *
StringFindLast(const wchar_t *haystack, wchar_t needle)
{
	return wcsrchr(haystack, needle);
}

gcc_pure gcc_nonnull_all
static inline wchar_t *
StringFindLast(wchar_t *haystack, wchar_t needle)
{
	return wcsrchr(haystack, needle);
}

gcc_nonnull_all
static inline void
UnsafeCopyString(wchar_t *dest, const wchar_t *src)
{
	wcscpy(dest, src);
}

gcc_nonnull_all
static inline wchar_t *
UnsafeCopyStringP(wchar_t *dest, const wchar_t *src)
{
#if defined(WIN32) || defined(__BIONIC__)
  /* emulate wcpcpy() */
  UnsafeCopyString(dest, src);
  return dest + StringLength(dest);
#else
  return wcpcpy(dest, src);
#endif
}

/**
 * Checks whether str1 and str2 are equal.
 * @param str1 String 1
 * @param str2 String 2
 * @return True if equal, False otherwise
 */
gcc_pure gcc_nonnull_all
static inline bool
StringIsEqual(const wchar_t *str1, const wchar_t *str2)
{
	return wcscmp(str1, str2) == 0;
}

/**
 * Checks whether #a and #b are equal.
 */
gcc_pure gcc_nonnull_all
static inline bool
StringIsEqual(const wchar_t *a, const wchar_t *b, size_t length)
{
	return wcsncmp(a, b, length) == 0;
}

#ifndef __BIONIC__

gcc_malloc gcc_nonnull_all
static inline wchar_t *
DuplicateString(const wchar_t *p)
{
	return wcsdup(p);
}

#endif

#endif
