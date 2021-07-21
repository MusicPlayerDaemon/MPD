/*
 * Copyright 2013-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef RUNTIME_ERROR_HXX
#define RUNTIME_ERROR_HXX

#include <stdexcept> // IWYU pragma: export
#include <utility>

#include <stdio.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
// TODO: fix this warning properly
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

template<typename... Args>
static inline std::runtime_error
FormatRuntimeError(const char *fmt, Args&&... args) noexcept
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
	return std::runtime_error(buffer);
}

template<typename... Args>
inline std::invalid_argument
FormatInvalidArgument(const char *fmt, Args&&... args) noexcept
{
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
	return std::invalid_argument(buffer);
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
