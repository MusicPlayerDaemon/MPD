// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <string_view>

/**
 * Skips whitespace at the beginning of the string, and returns the
 * first non-whitespace character.  If the string has no
 * non-whitespace characters, then a pointer to the NULL terminator is
 * returned.
 */
[[gnu::pure]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
const char *
StripLeft(const char *p) noexcept;

[[gnu::pure]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
static inline char *
StripLeft(char *p) noexcept
{
	return const_cast<char *>(StripLeft((const char *)p));
}

/**
 * Skips whitespace at the beginning of the string, and returns the
 * first non-whitespace character or the end pointer.
 */
[[gnu::pure]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
const char *
StripLeft(const char *p, const char *end) noexcept;

[[gnu::pure]]
std::string_view
StripLeft(std::string_view s) noexcept;

/**
 * Determine the string's end as if it was stripped on the right side.
 */
[[gnu::pure]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
const char *
StripRight(const char *p, const char *end) noexcept;

/**
 * Determine the string's end as if it was stripped on the right side.
 */
[[gnu::pure]] [[gnu::returns_nonnull]] [[gnu::nonnull]]
static inline char *
StripRight(char *p, char *end) noexcept
{
	return const_cast<char *>(StripRight((const char *)p,
					     (const char *)end));
}

/**
 * Determine the string's length as if it was stripped on the right
 * side.
 */
[[gnu::pure]] [[gnu::nonnull]]
std::size_t
StripRight(const char *p, std::size_t length) noexcept;

/**
 * Strip trailing whitespace by null-terminating the string.
 */
[[gnu::nonnull]]
void
StripRight(char *p) noexcept;

[[gnu::pure]]
std::string_view
StripRight(std::string_view s) noexcept;

/**
 * Skip whitespace at the beginning and terminate the string after the
 * last non-whitespace character.
 */
[[gnu::returns_nonnull]] [[gnu::nonnull]]
char *
Strip(char *p) noexcept;

[[gnu::pure]]
std::string_view
Strip(std::string_view s) noexcept;
