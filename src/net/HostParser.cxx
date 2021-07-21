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

#include "HostParser.hxx"
#include "util/CharUtil.hxx"

#include <cstring>

static inline bool
IsValidHostnameChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '-' || ch == '.' ||
		ch == '*'; /* for wildcards */
}

static inline bool
IsValidScopeChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) ||
		ch == '-' || ch == '_';
}

static const char *
FindScopeEnd(const char *p) noexcept
{
	if (*p == '%' && IsValidScopeChar(p[1])) {
		p += 2;
		while (IsValidScopeChar(*p))
			++p;
	}

	return p;
}

static inline bool
IsValidIPv6Char(char ch) noexcept
{
	return IsDigitASCII(ch) ||
		(ch >= 'a' && ch <= 'f') ||
		(ch >= 'A' && ch <= 'F') ||
		ch == ':';
}

static const char *
FindIPv6End(const char *p) noexcept
{
	while (IsValidIPv6Char(*p))
		++p;

	/* allow "%scope" after numeric IPv6 address */
	p = FindScopeEnd(p);

	return p;
}

ExtractHostResult
ExtractHost(const char *src) noexcept
{
	ExtractHostResult result{nullptr, src};
	const char *hostname;

	if (IsValidHostnameChar(*src)) {
		const char *colon = nullptr;

		hostname = src++;

		while (IsValidHostnameChar(*src) || *src == ':') {
			if (*src == ':') {
				if (colon != nullptr) {
					/* found a second colon: assume it's an IPv6
					   address */
					result.end = FindIPv6End(src + 1);
					result.host = {hostname, result.end};
					return result;
				} else
					/* remember the position of the first colon */
					colon = src;
			}

			++src;
		}

		if (colon != nullptr)
			/* hostname ends at colon */
			src = colon;

		result.end = src;
		result.host = {hostname, result.end};
	} else if (src[0] == ':' && src[1] == ':') {
		/* IPv6 address beginning with "::" */
		result.end = FindIPv6End(src + 2);
		result.host = {src, result.end};
	} else if (src[0] == '[') {
		/* "[hostname]:port" (IPv6?) */

		hostname = ++src;
		const char *end = std::strchr(hostname, ']');
		if (end == nullptr || end == hostname)
			/* failed, return nullptr */
			return result;

		result.host = {hostname, end};
		result.end = end + 1;
	} else {
		/* failed, return nullptr */
	}

	return result;
}
