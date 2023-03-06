// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <forward_list>
#include <string_view>

/**
 * Split a string at a certain separator character into sub strings
 * and returns a list of these.
 *
 * Two consecutive separator characters result in an empty string in
 * the list.
 *
 * An empty input string, as a special case, results in an empty list
 * (and not a list with an empty string).
 */
std::forward_list<std::string_view>
SplitString(std::string_view s, char separator, bool strip=true) noexcept;
