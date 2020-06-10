/*
 * Copyright (C) 2010-2015 Max Kellermann <max.kellermann@gmail.com>
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
