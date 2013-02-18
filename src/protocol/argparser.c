/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "argparser.h"
#include "result.h"

#include <glib.h>
#include <stdlib.h>

bool
check_uint32(struct client *client, uint32_t *dst, const char *s)
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
check_int(struct client *client, int *value_r, const char *s)
{
	char *test;
	long value;

	value = strtol(s, &test, 10);
	if (test == s || *test != '\0') {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

#if G_MAXLONG > G_MAXINT
	if (value < G_MININT || value > G_MAXINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r = (int)value;
	return true;
}

bool
check_range(struct client *client, unsigned *value_r1, unsigned *value_r2,
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
		*value_r2 = G_MAXUINT;
		return true;
	}

	if (value < 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Number is negative: %s", s);
		return false;
	}

#if G_MAXLONG > G_MAXUINT
	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}
#endif

	*value_r1 = (unsigned)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			command_error(client, ACK_ERROR_ARG,
				      "Integer or range expected: %s", s);
			return false;
		}

		if (test == test2)
			value = G_MAXUINT;

		if (value < 0) {
			command_error(client, ACK_ERROR_ARG,
				      "Number is negative: %s", s);
			return false;
		}

#if G_MAXLONG > G_MAXUINT
		if (value > G_MAXUINT) {
			command_error(client, ACK_ERROR_ARG,
				      "Number too large: %s", s);
			return false;
		}
#endif
		*value_r2 = (unsigned)value;
	} else {
		*value_r2 = (unsigned)value + 1;
	}

	return true;
}

bool
check_unsigned(struct client *client, unsigned *value_r, const char *s)
{
	unsigned long value;
	char *endptr;

	value = strtoul(s, &endptr, 10);
	if (endptr == s || *endptr != 0) {
		command_error(client, ACK_ERROR_ARG,
			      "Integer expected: %s", s);
		return false;
	}

	if (value > G_MAXUINT) {
		command_error(client, ACK_ERROR_ARG,
			      "Number too large: %s", s);
		return false;
	}

	*value_r = (unsigned)value;
	return true;
}

bool
check_bool(struct client *client, bool *value_r, const char *s)
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
check_float(struct client *client, float *value_r, const char *s)
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
