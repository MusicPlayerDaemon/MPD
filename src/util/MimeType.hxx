// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef MIME_TYPE_HXX
#define MIME_TYPE_HXX

#include <string>
#include <string_view>
#include <map>

/**
 * Extract the part of the MIME type before the parameters, i.e. the
 * part before the semicolon.  If there is no semicolon, it returns
 * the string as-is.
 */
[[gnu::pure]]
std::string_view
GetMimeTypeBase(std::string_view s) noexcept;

/**
 * Parse the parameters from a MIME type string.  Parameters are
 * separated by semicolon.  Example:
 *
 * "foo/bar; param1=value1; param2=value2"
 */
std::map<std::string, std::string, std::less<>>
ParseMimeTypeParameters(std::string_view mime_type) noexcept;

#endif
