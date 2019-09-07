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

#ifndef URI_EXTRACT_HXX
#define URI_EXTRACT_HXX

#include "Compiler.h"

struct StringView;

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
StringView
uri_get_scheme(const char *uri) noexcept;

gcc_pure
bool
uri_is_relative_path(const char *uri) noexcept;

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
 * Returns the URI fragment, i.e. the portion after the '#', but
 * without the '#'.  If there is no '#', this function returns
 * nullptr; if there is a '#' but no fragment text, it returns an
 * empty StringView.
 */
gcc_pure gcc_nonnull_all
const char *
uri_get_fragment(const char *uri) noexcept;

#endif
