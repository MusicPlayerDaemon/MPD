// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FORMAT_STRING_HXX
#define MPD_FORMAT_STRING_HXX

#include "Compiler.h"

#include <cstdarg>

class AllocatedString;

/**
 * Format into an #AllocatedString.
 */
[[gnu::nonnull]]
AllocatedString
FormatStringV(const char *fmt, std::va_list args) noexcept;

/**
 * Format into an #AllocatedString.
 */
[[gnu::nonnull(1)]]
gcc_printf(1,2)
AllocatedString
FormatString(const char *fmt, ...) noexcept;

#endif
