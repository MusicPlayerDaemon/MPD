// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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

constexpr bool
IsLowerAlphaNumericASCII(wchar_t ch) noexcept
{
	return IsLowerAlphaASCII(ch) || IsDigitASCII(ch);
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
