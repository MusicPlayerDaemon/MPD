/*
 * Copyright (C) 2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef HEX_FORMAT_HXX
#define HEX_FORMAT_HXX

#include "ConstBuffer.hxx"
#include "StringBuffer.hxx"
#include "Compiler.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Format the given byte sequence into a null-terminated hexadecimal
 * string.
 *
 * @param dest the destination buffer; must be large enough to hold
 * all hex digits plus the null terminator
 * @return a pointer to the generated null terminator
 */
char *
HexFormat(char *dest, ConstBuffer<uint8_t> src) noexcept;

/**
 * Like HexFormat(), but return a #StringBuffer with exactly the
 * required size.
 */
template<size_t size>
gcc_pure
auto
HexFormatBuffer(const uint8_t *src) noexcept
{
	StringBuffer<size * 2 + 1> dest;
	HexFormat(dest.data(), {src, size});
	return dest;
}

#endif
