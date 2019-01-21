/*
 * Copyright (C) 2011-2017 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CHAR_UTIL_HXX
#define CHAR_UTIL_HXX

#ifdef _UNICODE
#include "WCharUtil.hxx"
#endif

constexpr bool
IsASCII(const unsigned char ch)
{
	return ch < 0x80;
}

constexpr bool
IsASCII(const char ch)
{
	return IsASCII((unsigned char)ch);
}

constexpr bool
IsWhitespaceOrNull(const char ch)
{
	return (unsigned char)ch <= 0x20;
}

constexpr bool
IsWhitespaceNotNull(const char ch)
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
IsWhitespaceFast(const char ch)
{
	return IsWhitespaceOrNull(ch);
}

constexpr bool
IsPrintableASCII(char ch)
{
	return (signed char)ch >= 0x20;
}

constexpr bool
IsDigitASCII(char ch)
{
	return ch >= '0' && ch <= '9';
}

constexpr bool
IsUpperAlphaASCII(char ch)
{
	return ch >= 'A' && ch <= 'Z';
}

constexpr bool
IsLowerAlphaASCII(char ch)
{
	return ch >= 'a' && ch <= 'z';
}

constexpr bool
IsAlphaASCII(char ch)
{
	return IsUpperAlphaASCII(ch) || IsLowerAlphaASCII(ch);
}

constexpr bool
IsAlphaNumericASCII(char ch)
{
	return IsAlphaASCII(ch) || IsDigitASCII(ch);
}

/**
 * Convert the specified ASCII character (0x00..0x7f) to upper case.
 * Unlike toupper(), it ignores the system locale.
 */
constexpr char
ToUpperASCII(char ch)
{
	return ch >= 'a' && ch <= 'z'
		? (ch - ('a' - 'A'))
		: ch;
}

/**
 * Convert the specified ASCII character (0x00..0x7f) to lower case.
 * Unlike tolower(), it ignores the system locale.
 */
constexpr char
ToLowerASCII(char ch)
{
	return ch >= 'A' && ch <= 'Z'
		? (ch + ('a' - 'A'))
		: ch;
}

#endif
