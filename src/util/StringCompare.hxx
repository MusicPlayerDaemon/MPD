/*
 * Copyright (C) 2013-2015 Max Kellermann <max@duempel.org>
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

#ifndef STRING_COMPARE_HXX
#define STRING_COMPARE_HXX

#include "Compiler.h"

#ifdef _UNICODE
#include "WStringCompare.hxx"
#endif

gcc_pure
bool
StringStartsWith(const char *haystack, const char *needle);

gcc_pure
bool
StringEndsWith(const char *haystack, const char *needle);

/**
 * Returns the portion of the string after a prefix.  If the string
 * does not begin with the specified prefix, this function returns
 * nullptr.
 */
gcc_pure gcc_nonnull_all
const char *
StringAfterPrefix(const char *string, const char *prefix);

/**
 * Check if the given string ends with the specified suffix.  If yes,
 * returns the position of the suffix, and nullptr otherwise.
 */
gcc_pure
const char *
FindStringSuffix(const char *p, const char *suffix);

#endif
