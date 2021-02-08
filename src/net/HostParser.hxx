/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#ifndef NET_HOST_PARSER_HXX
#define NET_HOST_PARSER_HXX

#include "util/StringView.hxx"

/**
 * Result type for ExtractHost().
 */
struct ExtractHostResult {
	/**
	 * The host part of the address.
	 *
	 * If nothing was parsed, then this is nullptr.
	 */
	StringView host;

	/**
	 * Pointer to the first character that was not parsed.  On
	 * success, this is usually a pointer to the zero terminator or to
	 * a colon followed by a port number.
	 *
	 * If nothing was parsed, then this is a pointer to the given
	 * source string.
	 */
	const char *end;

	constexpr bool HasFailed() const noexcept {
		return host == nullptr;
	}
};

/**
 * Extract the host from a string in the form "IP:PORT" or
 * "[IPv6]:PORT".  Stops at the first invalid character (e.g. the
 * colon).
 *
 * @param src the input string
 */
[[gnu::pure]]
ExtractHostResult
ExtractHost(const char *src) noexcept;

#endif
