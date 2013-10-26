/*
 * Copyright (C) 2009-2013 Max Kellermann <max@duempel.org>
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

#endif
