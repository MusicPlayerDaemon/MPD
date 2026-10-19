// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string>

/**
 * Is this a relative path in canonical form?
 *
 * - non-empty
 * - does not begin or end with a slash
 * - no double slashes
 * - no "." or ".." segments
 * - no backslashes (on Windows)
 *
 * Such a path cannot refer to anything outside of the directory it is
 * relative to.
 */
[[gnu::pure]]
bool
UriIsCanonicalRelativePath(std::string_view uri) noexcept;

/**
 * Is this a relative path which consists of a (possibly empty) run of
 * leading "../" and "./" segments followed by a canonical relative
 * path (see UriIsCanonicalRelativePath())?
 *
 * @param max_up the maximum number of leading "../" segments (i.e.
 * the depth of the base directory), so the URI cannot refer to
 * something above the root
 */
[[gnu::pure]]
bool
UriIsRelativePathWithinDepth(std::string_view uri, unsigned max_up) noexcept;

/**
 * Does the given relative path (which may contain "." and ".."
 * segments anywhere) stay inside the storage root?
 *
 * Unlike UriIsRelativePathWithinDepth(), this allows ".." segments to
 * appear anywhere, not just at the beginning.
 *
 * @param depth the allowed number of remaining ".." segments
 */
[[gnu::pure]]
bool
UriStaysInsideDepth(std::string_view path, unsigned depth) noexcept;

/**
 * Removes HTTP username and password from the URI.  This may be
 * useful for displaying an URI without disclosing secrets.  Returns
 * an empty string if nothing needs to be removed, or if the URI is
 * not recognized.
 */
[[gnu::pure]]
std::string
UriRemoveAuth(std::string_view uri) noexcept;

/**
 * Remove dot segments in the URI.  For example, UriSquashDotSegments
 * ("foo/bar/.././")=="foo/".
 */
[[gnu::pure]]
std::string
UriSquashDotSegments(std::string_view uri) noexcept;
