// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef STRING_FORMAT_HXX
#define STRING_FORMAT_HXX

#include "StringBuffer.hxx" // IWYU pragma: export

#include <stdio.h>

template<typename... Args>
static inline void
StringFormat(char *buffer, std::size_t size,
	     const char *fmt, Args&&... args) noexcept
{
	snprintf(buffer, size, fmt, args...);
}

template<std::size_t CAPACITY, typename... Args>
static inline void
StringFormat(StringBuffer<CAPACITY> &buffer,
	     const char *fmt, Args&&... args) noexcept
{
	StringFormat(buffer.data(), buffer.capacity(), fmt, args...);
}

template<std::size_t CAPACITY, typename... Args>
static inline StringBuffer<CAPACITY>
StringFormat(const char *fmt, Args&&... args) noexcept
{
	StringBuffer<CAPACITY> result;
	StringFormat(result, fmt, args...);
	return result;
}

template<typename... Args>
static inline void
StringFormatUnsafe(char *buffer, const char *fmt, Args&&... args) noexcept
{
	sprintf(buffer, fmt, args...);
}

#endif
