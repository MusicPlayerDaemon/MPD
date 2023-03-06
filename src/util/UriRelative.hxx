// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string>
#include <string_view>

/**
 * Check whether #child specifies a resource "inside" the directory
 * specified by #parent.  If the strings are equal, the function
 * returns false.
 */
[[gnu::pure]] [[gnu::nonnull]]
bool
uri_is_child(const char *parent, const char *child) noexcept;

[[gnu::pure]] [[gnu::nonnull]]
bool
uri_is_child_or_same(const char *parent, const char *child) noexcept;

/**
 * Translate the given URI in the context of #base.  For example,
 * uri_apply_base("foo", "http://bar/a/")=="http://bar/a/foo".
 */
[[gnu::pure]]
std::string
uri_apply_base(std::string_view uri, std::string_view base) noexcept;

[[gnu::pure]]
std::string
uri_apply_relative(std::string_view relative_uri,
		   std::string_view base_uri) noexcept;
