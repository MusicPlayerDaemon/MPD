// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>

#include <stdlib.h>

static inline unsigned
ParseUnsigned(const char *p, char **endptr=nullptr, int base=10) noexcept
{
	assert(p != nullptr);

	return (unsigned)strtoul(p, endptr, base);
}

static inline int
ParseInt(const char *p, char **endptr=nullptr, int base=10) noexcept
{
	assert(p != nullptr);

	return (int)strtol(p, endptr, base);
}

static inline uint64_t
ParseUint64(const char *p, char **endptr=nullptr, int base=10) noexcept
{
	assert(p != nullptr);

	return strtoull(p, endptr, base);
}

static inline int64_t
ParseInt64(const char *p, char **endptr=nullptr, int base=10) noexcept
{
	assert(p != nullptr);

	return strtoll(p, endptr, base);
}

int64_t
ParseInt64(std::string_view s, const char **endptr_r=nullptr, int base=10) noexcept;

static inline double
ParseDouble(const char *p, char **endptr=nullptr) noexcept
{
	assert(p != nullptr);

	return (double)strtod(p, endptr);
}

static inline float
ParseFloat(const char *p, char **endptr=nullptr) noexcept
{
	return strtof(p, endptr);
}
