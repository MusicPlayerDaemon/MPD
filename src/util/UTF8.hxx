// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef UTF8_HXX
#define UTF8_HXX

#include <cstddef>

/**
 * Is this a valid UTF-8 string?
 */
[[gnu::pure]] [[gnu::nonnull]]
bool
ValidateUTF8(const char *p) noexcept;

/**
 * @return the number of the sequence beginning with the given
 * character, or 0 if the character is not a valid start byte
 */
[[gnu::const]]
std::size_t
SequenceLengthUTF8(char ch) noexcept;

/**
 * @return the number of the first sequence in the given string, or 0
 * if the sequence is malformed
 */
[[gnu::pure]]
std::size_t
SequenceLengthUTF8(const char *p) noexcept;

/**
 * Convert the specified string from ISO-8859-1 to UTF-8.
 *
 * @return the UTF-8 version of the source string; may return #src if
 * there are no non-ASCII characters; returns nullptr if the destination
 * buffer is too small
 */
[[gnu::pure]]  [[gnu::nonnull]]
const char *
Latin1ToUTF8(const char *src, char *buffer, std::size_t buffer_size) noexcept;

/**
 * Convert the specified Unicode character to UTF-8 and write it to
 * the buffer.  buffer must have a length of at least 6!
 *
 * @return a pointer to the buffer plus the added bytes(s)
 */
[[gnu::nonnull]]
char *
UnicodeToUTF8(unsigned ch, char *buffer) noexcept;

/**
 * Returns the number of characters in the string.  This is different
 * from strlen(), which counts the number of bytes.
 */
[[gnu::pure]] [[gnu::nonnull]]
std::size_t
LengthUTF8(const char *p) noexcept;

#endif
