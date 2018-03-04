/*
 * Copyright (C) 2009-2013 Max Kellermann <max.kellermann@gmail.com>
 * http://www.musicpd.org
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

#ifndef NUMBER_PARSER_HXX
#define NUMBER_PARSER_HXX

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <limits>

#include <string.h>

static inline bool
ParseBool(const char *p, char **endptr=nullptr)
{
	assert(p != nullptr);

	if (strcasecmp(p, "on") == 0
		|| strcasecmp(p, "enable") == 0
		|| strcasecmp(p, "yes") == 0
		|| strcasecmp(p, "true") == 0) {
		if (endptr != nullptr)
			*endptr = const_cast<char*>(p+strlen(p));
		return true;
	}
	if (strcasecmp(p, "off") == 0
		|| strcasecmp(p, "disable") == 0
		|| strcasecmp(p, "no") == 0
		|| strcasecmp(p, "false") == 0) {
		if (endptr != nullptr)
			*endptr = const_cast<char*>(p+strlen(p));
		return false;
	}

	return strtol(p, endptr, 10);
}

static inline unsigned
ParseUnsigned(const char *p, char **endptr=nullptr, int base=10)
{
	assert(p != nullptr);

	return (unsigned)strtoul(p, endptr, base);
}

static inline int
ParseInt(const char *p, char **endptr=nullptr, int base=10)
{
	assert(p != nullptr);

	return (int)strtol(p, endptr, base);
}

static inline uint64_t
ParseUint64(const char *p, char **endptr=nullptr, int base=10)
{
	assert(p != nullptr);

	return strtoull(p, endptr, base);
}

static inline int64_t
ParseInt64(const char *p, char **endptr=nullptr, int base=10)
{
	assert(p != nullptr);

	return strtoll(p, endptr, base);
}

static inline double
ParseDouble(const char *p, char **endptr=nullptr)
{
	assert(p != nullptr);

	return (double)strtod(p, endptr);
}

static inline float
ParseFloat(const char *p, char **endptr=nullptr)
{
	return (float)ParseDouble(p, endptr);
}

static inline bool
ParseIntRange(const char *p, int *value_r1, int *value_r2)
{
	char *test, *test2;
	long value;

	value = strtol(p, &test, 10);
	if (test == p || (*test != '\0' && *test != ':')) {
		return false;
	}

	if (int(value) > std::numeric_limits<int>::max()) {
		return false;
	}

	*value_r1 = (int)value;

	if (*test == ':') {
		value = strtol(++test, &test2, 10);
		if (*test2 != '\0') {
			return false;
		}

		if (test == test2)
			value = std::numeric_limits<int>::max();

		if (int(value) > std::numeric_limits<int>::max()) {
			return false;
		}

		*value_r2 = (int)value;
	} else {
		*value_r2 = -1;
	}

	return true;
}

#endif
