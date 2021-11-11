/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ArgParser.hxx"
#include "RangeArg.hxx"
#include "Ack.hxx"
#include "Chrono.hxx"
#include "util/NumberParser.hxx"

#include <stdio.h>
#include <stdlib.h>

static inline ProtocolError
MakeArgError(const char *msg, const char *value) noexcept
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s: %s", msg, value);
	return {ACK_ERROR_ARG, buffer};
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
