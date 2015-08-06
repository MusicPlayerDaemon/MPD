/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "config.h"
#include "ArgParser.hxx"
#include "Chrono.hxx"
#include "client/Response.hxx"

#include <stdlib.h>

bool
ParseCommandArg32(Response &r, uint32_t &value_r, const char *s)
{
	char *test;

	value_r = strtoul(s, &test, 10);
	if (test == s || *test != '\0') {
		r.FormatError(ACK_ERROR_ARG, "Integer expected: %s", s);
		return false;
	}
	return true;
}

bool
ParseCommandArg(Response &r, int &value_r, const char *s,
		int min_value, int max_value)
{
	char *test;
	long value;

	value = strtol(s, &test, 10);
	if (test == s || *test != '\0') {
		r.FormatError(ACK_ERROR_ARG, "Integer expected: %s", s);
		return false;
	}

	if (value < min_value || value > max_value) {
		r.FormatError(ACK_ERROR_ARG, "Number too large: %s", s);
		return false;
	}

	value_r = (int)value;
	return true;
}

bool
ParseCommandArg(Response &r, int &value_r, const char *s)
{
	return ParseCommandArg(r, value_r, s,
			       std::numeric_limits<int>::min(),
			       std::numeric_limits<int>::max());
}

bool
ParseCommandArg(Response &r, RangeArg &value_r, const char *s)
{
	char *test, *test2;
	long value;

	value = strtol(s, &test, 10);
	if (test == s || (*test != '\0' && *test != ':')) {
		r.FormatError(ACK_ERROR_ARG,
			      "Integer or range expected: %s", s);
		return false;
	}

	if (value == -1 && *test == 0) {
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		value_r.start = 0;
		value_r.end = std::numeric_limits<int>::max();
		return true;
	}

	if (value < 0) {
		r.FormatError(ACK_ERROR_ARG, "Number is negative: %s", s);
		return false;
	}

	if (unsigned(value) > std::numeric_limits<unsigned>::max()) {
		r.FormatError(ACK_ERROR_ARG, "Number too large: %s", s);
		return false;
	}

	value_r.start = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			r.FormatError(ACK_ERROR_ARG,
				      "Integer or range expected: %s", s);
			return false;
		}

		if (test == test2)
			value = std::numeric_limits<int>::max();

		if (value < 0) {
			r.FormatError(ACK_ERROR_ARG,
				      "Number is negative: %s", s);
			return false;
		}

		if (unsigned(value) > std::numeric_limits<unsigned>::max()) {
			r.FormatError(ACK_ERROR_ARG,
				      "Number too large: %s", s);
			return false;
		}

		value_r.end = (unsigned)value;
	} else {
		value_r.end = (unsigned)value + 1;
	}

	return true;
}

bool
ParseCommandArg(Response &r, unsigned &value_r, const char *s,
		unsigned max_value)
{
	unsigned long value;
	char *endptr;

	value = strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0) {
		r.FormatError(ACK_ERROR_ARG, "Integer expected: %s", s);
		return false;
	}

	if (value > max_value) {
		r.FormatError(ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	value_r = (unsigned)value;
	return true;
}

bool
ParseCommandArg(Response &r, unsigned &value_r, const char *s)
{
	return ParseCommandArg(r, value_r, s,
			       std::numeric_limits<unsigned>::max());
}

bool
ParseCommandArg(Response &r, bool &value_r, const char *s)
{
	long value;
	char *endptr;

	value = strtol(s, &endptr, 10);
	if (endptr == s || *endptr != 0 || (value != 0 && value != 1)) {
		r.FormatError(ACK_ERROR_ARG,
			      "Boolean (0/1) expected: %s", s);
		return false;
	}

	value_r = !!value;
	return true;
}

bool
ParseCommandArg(Response &r, float &value_r, const char *s)
{
	float value;
	char *endptr;

	value = strtof(s, &endptr);
	if (endptr == s || *endptr != 0) {
		r.FormatError(ACK_ERROR_ARG, "Float expected: %s", s);
		return false;
	}

	value_r = value;
	return true;
}

bool
ParseCommandArg(Response &r, SongTime &value_r, const char *s)
{
	float value;
	bool success = ParseCommandArg(r, value, s) && value >= 0;
	if (success)
		value_r = SongTime::FromS(value);

	return success;
}

bool
ParseCommandArg(Response &r, SignedSongTime &value_r, const char *s)
{
	float value;
	bool success = ParseCommandArg(r, value, s);
	if (success)
		value_r = SignedSongTime::FromS(value);

	return success;
}
