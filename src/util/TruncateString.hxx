// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef TRUNCATE_STRING_HXX
#define TRUNCATE_STRING_HXX

#include <cstddef>

/**
 * Copy a string.  If the buffer is too small, then the string is
 * truncated.  This is a safer version of strncpy().
 *
 * @param size the size of the destination buffer (including the null
 * terminator)
 * @return a pointer to the null terminator
 */
[[gnu::nonnull]]
char *
CopyTruncateString(char *dest, const char *src, size_t size) noexcept;

#endif
