/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Result.hxx"
#include "Chrono.hxx"

#include <limits>

#include <stdlib.h>

bool
check_uint32(Client &client, uint32_t *dst, const char *s)
{
	char *test;

	*dst = strtoul(s, &test, 10);
	if (test == s || *test != '\0') {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}
	return true;
}

bool
check_int(Client &client, int *value_r, const char *s)
{
	char *test;
	long value;

	value = strtol(s, &test, 10);
	if (test == s || *test != '\0') {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

	if (value < std::numeric_limits<int>::min() ||
	    value > std::numeric_limits<int>::max()) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r = (int)value;
	return true;
}

bool
check_range(Client &client, unsigned *value_r1, unsigned *value_r2,
	    const char *s)
{
	char *test, *test2;
	long value;

	value = strtol(s, &test, 10);
	if (test == s || (*test != '\0' && *test != ':')) {
		command_error(client, ACK_ERROR_ARG,
			      "Integer or range expected: %s", s);
		return false;
	}

	if (value == -1 && *test == 0) {
		/* compatibility with older MPD versions: specifying
		   "-1" makes MPD display the whole list */
		*value_r1 = 0;
		*value_r2 = std::numeric_limits<int>::max();
		return true;
	}

	if (value < 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Number is negative: %s", s);
		return false;
	}

	if (unsigned(value) > std::numeric_limits<unsigned>::max()) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r1 = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			command_error(client, ACK_ERROR_ARG,
				      "Integer or range expected: %s", s);
			return false;
		}

		if (test == test2)
			value = std::numeric_limits<int>::max();

		if (value < 0) {
			command_error(client, ACK_ERROR_ARG,
				      "Number is negative: %s", s);
			return false;
		}

		if (unsigned(value) > std::numeric_limits<unsigned>::max()) {
			command_error(client, ACK_ERROR_ARG,
				      "Number too large: %s", s);
			return false;
		}

		*value_r2 = (unsigned)value;
	} else {
		*value_r2 = (unsigned)value + 1;
	}

	return true;
}

bool
check_unsigned(Client &client, unsigned *value_r, const char *s)
{
	unsigned long value;
	char *endptr;

	value = strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

	if (value > std::numeric_limits<unsigned>::max()) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r = (unsigned)value;
	return true;
}

bool
check_bool(Client &client, bool *value_r, const char *s)
{
	long value;
	char *endptr;

	value = strtol(s, &endptr, 10);
	if (endptr == s || *endptr != 0 || (value != 0 && value != 1)) {
		command_error(client, ACK_ERROR_ARG,
			      "Boolean (0/1) expected: %s", s);
		return false;
	}

	*value_r = !!value;
	return true;
}

bool
check_float(Client &client, float *value_r, const char *s)
{
	float value;
	char *endptr;

	value = strtof(s, &endptr);
	if (endptr == s || *endptr != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Float expected: %s", s);
		return false;
	}

	*value_r = value;
	return true;
}

bool
ParseCommandArg(Client &client, SongTime &value_r, const char *s)
{
	float value;
	bool success = check_float(client, &value, s) && value >= 0;
	if (success)
		value_r = SongTime::FromS(value);

	return success;
}

bool
ParseCommandArg(Client &client, SignedSongTime &value_r, const char *s)
{
	float value;
	bool success = check_float(client, &value, s);
	if (success)
		value_r = SignedSongTime::FromS(value);

	return success;
}
