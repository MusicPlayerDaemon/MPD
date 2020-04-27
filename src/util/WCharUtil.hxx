/*
 * Copyright 2011-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef WCHAR_UTIL_HXX
#define WCHAR_UTIL_HXX

#include <wchar.h>

constexpr bool
IsASCII(const wchar_t ch) noexcept
{
	return (ch & ~0x7f) == 0;
}

constexpr bool
IsWhitespaceOrNull(const wchar_t ch) noexcept
{
	return (unsigned)ch <= 0x20;
}

constexpr bool
IsWhitespaceNotNull(const wchar_t ch) noexcept
{
	return ch > 0 && ch <= 0x20;
}

/**
 * Is the given character whitespace?  This calls the faster one of
 * IsWhitespaceOrNull() or IsWhitespaceNotNull().  Use this if you
 * want the fastest implementation, and you don't care if a null byte
 * matches.
 */
constexpr bool
IsWhitespaceFast(const wchar_t ch) noexcept
{
	return IsWhitespaceOrNull(ch);
}

/**
 * Is this a non-printable ASCII character?  Returns false for
 * non-ASCII characters.
 *
 * Note that this is not the opposide of IsNonPrintableASCII().
 */
constexpr bool
IsPrintableASCII(wchar_t ch) noexcept
{
	return IsASCII(ch) && ch >= 0x20;
}

/**
 * Is this a non-printable character?  Returns false for non-ASCII
 * characters.
 *
 * Note that this is not the opposide of IsPrintableASCII()
 */
constexpr bool
IsNonPrintableASCII(wchar_t ch) noexcept
{
	return (unsigned)ch < 0x20;
}

constexpr bool
IsDigitASCII(wchar_t ch) noexcept
{
	return ch >= '0' && ch <= '9';
}

constexpr bool
IsUpperAlphaASCII(wchar_t ch) noexcept
{
	return ch >= 'A' && ch <= 'Z';
}

constexpr bool
IsLowerAlphaASCII(wchar_t ch) noexcept
{
	return ch >= 'a' && ch <= 'z';
}

constexpr bool
IsAlphaASCII(wchar_t ch) noexcept
{
	return IsUpperAlphaASCII(ch) || IsLowerAlphaASCII(ch);
}

constexpr bool
IsAlphaNumericASCII(wchar_t ch) noexcept
{
	return IsAlphaASCII(ch) || IsDigitASCII(ch);
}

/**
 * Convert the specified ASCII character (0x00..0x7f) to upper case.
 * Unlike toupper(), it ignores the system locale.
 */
constexpr wchar_t
ToUpperASCII(wchar_t ch) noexcept
{
	return ch >= 'a' && ch <= 'z'
		? (ch - ('a' - 'A'))
		: ch;
}

/**
 * Convert the specified ASCII character (0x00..0x7f) to lower case.
 * Unlike tolower(), it ignores the system locale.
 */
constexpr wchar_t
ToLowerASCII(wchar_t ch) noexcept
{
	return ch >= 'A' && ch <= 'Z'
		? (ch + ('a' - 'A'))
		: ch;
}

#endif
