// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string_view>

/**
 * Result type for ExtractHost().
 */
struct ExtractHostResult {
	/**
	 * The host part of the address.
	 *
	 * If nothing was parsed, then this is nullptr.
	 */
	std::string_view host;

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
		return host.data() == nullptr;
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
