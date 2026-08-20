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
UriHasScheme(std::string_view uri) noexcept;

/**
 * Returns the scheme name of the specified URI, or an empty string.
 */
[[gnu::pure]]
std::string_view
UriGetScheme(std::string_view uri) noexcept;

/**
 * Return the URI part after the protocol specification (and after the
 * double slash).
 */
[[gnu::pure]]
std::string_view
UriAfterScheme(std::string_view uri) noexcept;

[[gnu::pure]]
bool
UriIsRelativePath(const char *uri) noexcept;

/**
 * Returns the URI path (including query and fragment) or nullptr if
 * the given URI has no path.
 */
[[gnu::pure]]
std::string_view
UriPathQueryFragment(std::string_view uri) noexcept;

/**
 * Returns the URI path (excluding query and fragment) or nullptr if
 * the given URI has no path.
 */
[[gnu::pure]]
std::string_view
UriGetPath(std::string_view uri) noexcept;

[[gnu::pure]]
std::string_view
UriGetSuffix(std::string_view uri) noexcept;

/**
 * Returns the URI fragment, i.e. the portion after the '#', but
 * without the '#'.  If there is no '#', this function returns
 * nullptr; if there is a '#' but no fragment text, it returns an
 * empty std::string_view.
 */
[[gnu::pure]] [[gnu::nonnull]]
const char *
UriGetFragment(const char *uri) noexcept;
