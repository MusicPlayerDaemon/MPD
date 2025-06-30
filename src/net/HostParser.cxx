// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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

static constexpr std::string_view
SV(const char *begin, const char *end) noexcept
{
#if __cplusplus >= 202002 && !defined(__clang__)
	return {begin, end};
#else
	/* kludge for libc++ which does not yet implement the C++20
	   iterator constructor */
	return {begin, std::size_t(end - begin)};
#endif
}

ExtractHostResult
ExtractHost(const char *src) noexcept
{
	ExtractHostResult result{{}, src};
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
					result.host = SV(hostname, result.end);
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
		result.host = SV(hostname, result.end);
	} else if (src[0] == ':' && src[1] == ':') {
		/* IPv6 address beginning with "::" */
		result.end = FindIPv6End(src + 2);
		result.host = SV(src, result.end);
	} else if (src[0] == '[') {
		/* "[hostname]:port" (IPv6?) */

		hostname = ++src;
		const char *end = std::strchr(hostname, ']');
		if (end == nullptr || end == hostname)
			/* failed, return nullptr */
			return result;

		result.host = SV(hostname, end);
		result.end = end + 1;
	} else {
		/* failed, return nullptr */
	}

	return result;
}
