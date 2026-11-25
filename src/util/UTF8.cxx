// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "UTF8.hxx"
#include "CharUtil.hxx"
#include "Compiler.h"

#include <algorithm>
#include <cstdint>

/**
 * Is this a leading byte that is followed by 1 continuation byte?
 * (0xc0 and 0xc1 would be overlong.)
 */
static constexpr bool
IsLeading1(uint8_t ch) noexcept
{
	return ch >= 0xc2 && ch <= 0xdf;
}

static constexpr uint8_t
MakeLeading1(uint8_t value) noexcept
{
	return 0xc0 | value;
}

/**
 * Is this a leading byte that is followed by 2 continuation byte?
 */
static constexpr bool
IsLeading2(uint8_t ch) noexcept
{
	return (ch & 0xf0) == 0xe0;
}

static constexpr uint8_t
MakeLeading2(uint8_t value) noexcept
{
	return 0xe0 | value;
}

/**
 * Is this a leading byte that is followed by 3 continuation byte?
 * (0xf5 and above would exceed U+10FFFF.)
 */
static constexpr bool
IsLeading3(uint8_t ch) noexcept
{
	return ch >= 0xf0 && ch <= 0xf4;
}

static constexpr uint8_t
MakeLeading3(uint8_t value) noexcept
{
	return 0xf0 | value;
}

static constexpr uint8_t
MakeLeading4(uint8_t value) noexcept
{
	return 0xf8 | value;
}

static constexpr uint8_t
MakeLeading5(uint8_t value) noexcept
{
	return 0xfc | value;
}

/**
 * Is this any leading byte?
 */
static constexpr bool
IsLeading(uint8_t ch) noexcept
{
	return (ch & 0xc0) == 0xc0;
}

static constexpr bool
IsContinuation(uint8_t ch) noexcept
{
	return (ch & 0xc0) == 0x80;
}

/**
 * Is this a valid second byte after the given leading byte?  This
 * rejects overlong sequences, UTF-16 surrogates and code points
 * above U+10FFFF (RFC 3629, section 4).
 */
static constexpr bool
IsValidSecond(uint8_t leading, uint8_t ch) noexcept
{
	switch (leading) {
	case 0xe0:
		return ch >= 0xa0 && ch <= 0xbf;

	case 0xed:
		return ch >= 0x80 && ch <= 0x9f;

	case 0xf0:
		return ch >= 0x90 && ch <= 0xbf;

	case 0xf4:
		return ch >= 0x80 && ch <= 0x8f;

	default:
		return IsContinuation(ch);
	}
}

/**
 * Generate a continuation byte of the low 6 bit.
 */
static constexpr uint8_t
MakeContinuation(uint8_t value) noexcept
{
	return 0x80 | (value & 0x3f);
}

bool
ValidateUTF8(const char *p) noexcept
{
	for (; *p != 0; ++p) {
		uint8_t ch = *p;
		if (IsASCII(ch))
			continue;

		if (IsContinuation(ch))
			/* continuation without a prefix */
			return false;

		if (IsLeading1(ch)) {
			/* 1 continuation */
			if (!IsContinuation(*++p))
				return false;
		} else if (IsLeading2(ch)) {
			/* 2 continuations */
			if (!IsValidSecond(ch, *++p) || !IsContinuation(*++p))
				return false;
		} else if (IsLeading3(ch)) {
			/* 3 continuations */
			if (!IsValidSecond(ch, *++p) || !IsContinuation(*++p) ||
			    !IsContinuation(*++p))
				return false;
		} else
			return false;
	}

	return true;
}

bool
ValidateUTF8(std::string_view s) noexcept
{
	for (auto i = s.begin(), end = s.end(); i != end; ++i) {
		uint8_t ch = *i;
		if (IsASCII(ch))
			continue;

		if (IsContinuation(ch))
			/* continuation without a prefix */
			return false;

		if (IsLeading1(ch)) {
			/* 1 continuation */
			if (++i == end || !IsContinuation(*i))
				return false;
		} else if (IsLeading2(ch)) {
			/* 2 continuations */
			if (++i == end || !IsValidSecond(ch, *i) ||
			    ++i == end || !IsContinuation(*i))
				return false;
		} else if (IsLeading3(ch)) {
			/* 3 continuations */
			if (++i == end || !IsValidSecond(ch, *i) ||
			    ++i == end || !IsContinuation(*i) ||
			    ++i == end || !IsContinuation(*i))
				return false;
		} else
			return false;
	}

	return true;
}

std::size_t
SequenceLengthUTF8(char ch) noexcept
{
	if (IsASCII(ch))
		return 1;
	else if (IsLeading1(ch))
		/* 1 continuation */
		return 2;
	else if (IsLeading2(ch))
		/* 2 continuations */
		return 3;
	else if (IsLeading3(ch))
		/* 3 continuations */
		return 4;
	else
		/* continuation without a prefix or some other illegal
		   start byte */
		return 0;

}

template<std::size_t L>
struct CheckSequenceUTF8 {
	[[gnu::pure]]
	bool operator()(const char *p) const noexcept {
		return IsContinuation(*p) && CheckSequenceUTF8<L-1>()(p + 1);
	}
};

template<>
struct CheckSequenceUTF8<0U> {
	constexpr bool operator()([[maybe_unused]] const char *p) const noexcept {
		return true;
	}
};

template<std::size_t L>
[[gnu::pure]]
static std::size_t
InnerSequenceLengthUTF8(uint8_t leading, const char *p) noexcept
{
	return IsValidSecond(leading, *p) && CheckSequenceUTF8<L - 1>()(p + 1)
		? L + 1
		: 0U;
}

std::size_t
SequenceLengthUTF8(const char *p) noexcept
{
	const uint8_t ch = *p++;

	if (IsASCII(ch))
		return 1;
	else if (IsLeading1(ch))
		/* 1 continuation */
		return InnerSequenceLengthUTF8<1>(ch, p);
	else if (IsLeading2(ch))
		/* 2 continuations */
		return InnerSequenceLengthUTF8<2>(ch, p);
	else if (IsLeading3(ch))
		/* 3 continuations */
		return InnerSequenceLengthUTF8<3>(ch, p);
	else
		/* continuation without a prefix or some other illegal
		   start byte */
		return 0;
}

std::string_view
StripIncompleteUTF8(std::string_view s) noexcept
{
	const std::size_t original_size = s.size();

	while (true) {
		if (s.empty())
			return s;

		if (IsContinuation(s.back()))
			s.remove_suffix(1);
		else
			break;
	}

	if (IsLeading(s.back())) {
		if (SequenceLengthUTF8(s.back()) == original_size - s.size() + 1)
			/* the last sequence is complete */
			return {s.data(), original_size};

		s.remove_suffix(1);
	}

	return s;
}

[[gnu::pure]]
static const char *
FindNonASCIIOrZero(const char *p) noexcept
{
  while (*p != 0 && IsASCII(*p))
    ++p;
  return p;
}

const char *
Latin1ToUTF8(const char *gcc_restrict src, char *gcc_restrict buffer,
	     std::size_t buffer_size) noexcept
{
	const char *p = FindNonASCIIOrZero(src);
	if (*p == 0)
		/* everything is plain ASCII, we don't need to convert anything */
		return src;

	if ((std::size_t)(p - src) >= buffer_size)
		/* buffer too small */
		return nullptr;

	const char *const end = buffer + buffer_size;
	char *q = std::copy(src, p, buffer);

	while (*p != 0) {
		uint8_t ch = *p++;

		if (IsASCII(ch)) {
			*q++ = ch;

			if (q >= end)
				/* buffer too small */
				return nullptr;
		} else {
			if (q + 2 >= end)
				/* buffer too small */
				return nullptr;

			*q++ = MakeLeading1(ch >> 6);
			*q++ = MakeContinuation(ch);
		}
	}

	*q = 0;
	return buffer;
}

char *
UnicodeToUTF8(unsigned ch, char *q) noexcept
{
	if (ch < 0x80) [[likely]] {
		*q++ = (char)ch;
	} else if (ch < 0x800) [[likely]] {
		*q++ = MakeLeading1(ch >> 6);
		*q++ = MakeContinuation(ch);
	} else if (ch < 0x10000) {
		*q++ = MakeLeading2(ch >> 12);
		*q++ = MakeContinuation(ch >> 6);
		*q++ = MakeContinuation(ch);
	} else if (ch < 0x200000) {
		*q++ = MakeLeading3(ch >> 18);
		*q++ = MakeContinuation(ch >> 12);
		*q++ = MakeContinuation(ch >> 6);
		*q++ = MakeContinuation(ch);
	} else if (ch < 0x4000000) {
		*q++ = MakeLeading4(ch >> 24);
		*q++ = MakeContinuation(ch >> 18);
		*q++ = MakeContinuation(ch >> 12);
		*q++ = MakeContinuation(ch >> 6);
		*q++ = MakeContinuation(ch);
	} else if (ch < 0x80000000) {
		*q++ = MakeLeading5(ch >> 30);
		*q++ = MakeContinuation(ch >> 24);
		*q++ = MakeContinuation(ch >> 18);
		*q++ = MakeContinuation(ch >> 12);
		*q++ = MakeContinuation(ch >> 6);
		*q++ = MakeContinuation(ch);
	} else {
		// error
	}

	return q;
}

std::size_t
LengthUTF8(const char *p) noexcept
{
	/* this is a very naive implementation: it does not do any
	   verification, it just counts the bytes that are not a UTF-8
	   continuation */

	std::size_t n = 0;
	for (; *p != 0; ++p)
		if (!IsContinuation(*p))
			++n;
	return n;
}
