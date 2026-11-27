// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ArgParser.hxx"
#include "RangeArg.hxx"
#include "Ack.hxx"
#include "Chrono.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/CharUtil.hxx"
#include "util/CNumberParser.hxx"
#include "util/NumberParser.hxx"
#include "util/StringAPI.hxx"
#include "util/StringSplit.hxx"

#include <cerrno>

static inline ProtocolError
MakeArgError(const char *msg, const char *value) noexcept
{
	return {ACK_ERROR_ARG, FmtBuffer<256>("{}: {}", msg, value)};
}

/**
 * Parse an integer which spans the whole string.
 *
 * @param msg the error message for malformed input
 * @param arg the whole argument (for the error message)
 */
template<std::integral T>
static T
ParseIntegerArg(std::string_view s, const char *msg, const char *arg)
{
	T value;
	const auto [ptr, ec] = FromChars(s, value);
	if (ec == std::errc::result_out_of_range)
		throw MakeArgError("Number too large", arg);

	if (ec != std::errc{} || ptr != s.data() + s.size())
		throw MakeArgError(msg, arg);

	return value;
}

template<std::integral T>
static T
ParseIntegerArg(const char *s)
{
	return ParseIntegerArg<T>(s, "Integer expected", s);
}

uint32_t
ParseCommandArgU32(const char *s)
{
	return ParseIntegerArg<uint32_t>(s);
}

int
ParseCommandArgInt(const char *s, int min_value, int max_value)
{
	/* skip the plus sign (e.g. "volume +5"); strtol() used to
	   accept it, and std::from_chars() does not */
	const char *p = s;
	if (*p == '+' && IsDigitASCII(p[1]))
		++p;

	const auto value = ParseIntegerArg<int>(p, "Integer expected", s);
	if (value < min_value || value > max_value)
		throw MakeArgError("Number too large", s);

	return value;
}

int
ParseCommandArgInt(const char *s)
{
	return ParseCommandArgInt(s,
				  std::numeric_limits<int>::min(),
				  std::numeric_limits<int>::max());
}

/**
 * Parse one number of a range.
 */
static unsigned
ParseRangeValue(std::string_view s, const char *arg)
{
	const auto value = ParseIntegerArg<int>(s, "Integer or range expected", arg);
	if (value < 0)
		throw MakeArgError("Number is negative", arg);

	return value;
}

RangeArg
ParseCommandArgRange(const char *s)
{
	if (StringIsEqual(s, "-1"))
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		return RangeArg::All();

	const auto [start_s, end_s] = Split(std::string_view{s}, ':');
	const unsigned start = ParseRangeValue(start_s, s);

	if (end_s.data() == nullptr)
		return RangeArg::Single(start);

	if (end_s.empty())
		return RangeArg::OpenEnded(start);

	const RangeArg range{start, ParseRangeValue(end_s, s)};
	if (!range.IsWellFormed())
		throw MakeArgError("Malformed range", s);

	return range;
}

unsigned
ParseCommandArgUnsigned(const char *s, unsigned max_value)
{
	const auto value = ParseIntegerArg<unsigned>(s);
	if (value > max_value)
		throw MakeArgError("Number too large", s);

	return value;
}

unsigned
ParseCommandArgUnsigned(const char *s)
{
	return ParseCommandArgUnsigned(s,
				       std::numeric_limits<unsigned>::max());
}

bool
ParseCommandArgBool(const char *s)
{
	const auto value = ParseIntegerArg<unsigned>(s, "Boolean (0/1) expected", s);
	if (value > 1)
		throw MakeArgError("Boolean (0/1) expected", s);

	return value != 0;
}

float
ParseCommandArgFloat(const char *s)
{
	char *endptr;
	errno = 0;
	auto value = ParseFloat(s, &endptr);
	if (endptr == s || *endptr != 0)
		throw MakeArgError("Float expected", s);

	if (errno == ERANGE)
		throw MakeArgError("Number out of range", s);

	return value;
}

SongTime
ParseCommandArgSongTime(const char *s)
{
	auto value = ParseCommandArgFloat(s);
	if (value < 0)
		throw MakeArgError("Negative value not allowed", s);

	return SongTime::FromS(value);
}

SignedSongTime
ParseCommandArgSignedSongTime(const char *s)
{
	auto value = ParseCommandArgFloat(s);
	return SignedSongTime::FromS(value);
}
