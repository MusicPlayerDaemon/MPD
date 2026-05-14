// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string>

/**
 * Returns true if this is a safe "local" URI:
 *
 * - non-empty
 * - does not begin or end with a slash
 * - no double slashes
 */
[[gnu::pure]]
bool
uri_safe_local(std::string_view uri) noexcept;

/**
 * Removes HTTP username and password from the URI.  This may be
 * useful for displaying an URI without disclosing secrets.  Returns
 * an empty string if nothing needs to be removed, or if the URI is
 * not recognized.
 */
[[gnu::pure]]
std::string
uri_remove_auth(std::string_view uri) noexcept;

/**
 * Remove dot segments in the URI.  For example, uri_squash_dot_segments
 * ("foo/bar/.././")=="foo/".
 */
[[gnu::pure]]
std::string
uri_squash_dot_segments(std::string_view uri) noexcept;
