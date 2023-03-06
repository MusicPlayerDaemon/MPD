// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string_view>

/**
 * Checks whether the specified URI has a scheme in the form
 * "scheme://".
 */
[[gnu::pure]]
bool
uri_has_scheme(std::string_view uri) noexcept;

/**
 * Returns the scheme name of the specified URI, or an empty string.
 */
[[gnu::pure]]
std::string_view
uri_get_scheme(std::string_view uri) noexcept;

[[gnu::pure]]
bool
uri_is_relative_path(const char *uri) noexcept;

/**
 * Returns the URI path (including the query string) or nullptr if the
 * given URI has no path.
 */
[[gnu::pure]]
std::string_view
uri_get_path(std::string_view uri) noexcept;

[[gnu::pure]]
std::string_view
uri_get_suffix(std::string_view uri) noexcept;

/**
 * Returns the URI fragment, i.e. the portion after the '#', but
 * without the '#'.  If there is no '#', this function returns
 * nullptr; if there is a '#' but no fragment text, it returns an
 * empty std::string_view.
 */
[[gnu::pure]] [[gnu::nonnull]]
const char *
uri_get_fragment(const char *uri) noexcept;
