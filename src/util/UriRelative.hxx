/*
 * Copyright 2008-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef URI_RELATIVE_HXX
#define URI_RELATIVE_HXX

#include "Compiler.h"

#include <string>
#include <string_view>

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
uri_apply_base(std::string_view uri, std::string_view base) noexcept;

gcc_pure
std::string
uri_apply_relative(std::string_view relative_uri,
		   std::string_view base_uri) noexcept;

#endif
