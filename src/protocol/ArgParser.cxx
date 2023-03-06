// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ArgParser.hxx"
#include "RangeArg.hxx"
#include "Ack.hxx"
#include "Chrono.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/NumberParser.hxx"

#include <stdlib.h>

static inline ProtocolError
MakeArgError(const char *msg, const char *value) noexcept
{
	return {ACK_ERROR_ARG, FmtBuffer<256>("{}: {}", msg, value)};
}

uint32_t
ParseCommandArgU32(const char *s)
{
	char *test;
	auto value = strtoul(s, &test, 10);
	if (test == s || *test != '\0')
		throw MakeArgError("Integer expected", s);

	return value;
}

int
ParseCommandArgInt(const char *s, int min_value, int max_value)
{
	char *test;
	auto value = strtol(s, &test, 10);
	if (test == s || *test != '\0')
		throw MakeArgError("Integer expected", s);

	if (value < min_value || value > max_value)
		throw MakeArgError("Number too large", s);

	return (int)value;
}

int
ParseCommandArgInt(const char *s)
{
	return ParseCommandArgInt(s,
				  std::numeric_limits<int>::min(),
				  std::numeric_limits<int>::max());
}

RangeArg
ParseCommandArgRange(const char *s)
{
	char *test, *test2;
	auto value = strtol(s, &test, 10);
	if (test == s || (*test != '\0' && *test != ':'))
		throw MakeArgError("Integer or range expected", s);

	if (value == -1 && *test == 0)
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		return RangeArg::All();

	if (value < 0)
		throw MakeArgError("Number is negative", s);

	if (value > std::numeric_limits<int>::max())
		throw MakeArgError("Number too large", s);

	RangeArg range;
	range.start = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0')
			throw MakeArgError("Integer or range expected", s);

		if (test == test2)
			return RangeArg::OpenEnded(range.start);

		if (value < 0)
			throw MakeArgError("Number is negative", s);


		if (value > std::numeric_limits<int>::max())
			throw MakeArgError("Number too large", s);

		range.end = (unsigned)value;
	} else {
		return RangeArg::Single(range.start);
	}

	if (!range.IsWellFormed())
		throw MakeArgError("Malformed range", s);

	return range;
}

unsigned
ParseCommandArgUnsigned(const char *s, unsigned max_value)
{
	char *endptr;
	auto value = strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0)
		throw MakeArgError("Integer expected", s);

	if (value > max_value)
		throw MakeArgError("Number too large", s);

	return (unsigned)value;
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
	char *endptr;
	auto value = strtol(s, &endptr, 10);
	if (endptr == s || *endptr != 0 || (value != 0 && value != 1))
		throw MakeArgError("Boolean (0/1) expected", s);

	return !!value;
}

float
ParseCommandArgFloat(const char *s)
{
	char *endptr;
	auto value = ParseFloat(s, &endptr);
	if (endptr == s || *endptr != 0)
		throw MakeArgError("Float expected", s);

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
