/*
 * Copyright 2008-2019 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef URI_UTIL_HXX
#define URI_UTIL_HXX

#include "Compiler.h"

#include <string>

/**
 * Checks whether the specified URI has a scheme in the form
 * "scheme://".
 */
gcc_pure
bool
uri_has_scheme(const char *uri) noexcept;

/**
 * Returns the scheme name of the specified URI, or an empty string.
 */
gcc_pure
std::string
uri_get_scheme(const char *uri) noexcept;

/**
 * Returns the URI path (including the query string) or nullptr if the
 * given URI has no path.
 */
gcc_pure gcc_nonnull_all
const char *
uri_get_path(const char *uri) noexcept;

gcc_pure
const char *
uri_get_suffix(const char *uri) noexcept;

struct UriSuffixBuffer {
	char data[8];
};

/**
 * Returns the file name suffix, ignoring the query string.
 */
gcc_pure
const char *
uri_get_suffix(const char *uri, UriSuffixBuffer &buffer) noexcept;

/**
 * Returns true if this is a safe "local" URI:
 *
 * - non-empty
 * - does not begin or end with a slash
 * - no double slashes
 * - no path component begins with a dot
 */
gcc_pure
bool
uri_safe_local(const char *uri) noexcept;

/**
 * Removes HTTP username and password from the URI.  This may be
 * useful for displaying an URI without disclosing secrets.  Returns
 * an empty string if nothing needs to be removed, or if the URI is
 * not recognized.
 */
gcc_pure
std::string
uri_remove_auth(const char *uri) noexcept;

/**
 * Check whether #child specifies a resource "inside" the directory
 * specified by #parent.  If the strings are equal, the function
 * returns false.
 */
gcc_pure gcc_nonnull_all
bool
uri_is_child(const char *parent, const char *child) noexcept;

gcc_pure gcc_nonnull_all
bool
uri_is_child_or_same(const char *parent, const char *child) noexcept;

/**
 * Translate the given URI in the context of #base.  For example,
 * uri_apply_base("foo", "http://bar/a/")=="http://bar/a/foo".
 */
gcc_pure
std::string
uri_apply_base(const std::string &uri, const std::string &base) noexcept;

#endif
