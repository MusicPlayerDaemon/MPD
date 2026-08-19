// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string_view>

/**
 * Find the first query parameter with the given name and return its
 * raw value (without unescaping).
 *
 * @return the raw value (pointing into the #query_string parameter)
 * or nullptr if the parameter does not exist
 */
[[gnu::pure]]
std::string_view
UriFindRawQueryParameter(std::string_view query_string, std::string_view name) noexcept;
