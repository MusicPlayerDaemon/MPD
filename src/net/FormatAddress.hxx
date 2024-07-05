// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

class SocketAddress;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer.
 *
 * @return true on success
 */
bool
ToString(std::span<char> buffer, SocketAddress address) noexcept;

/**
 * Like ToString() above, but return the string pointer (or on error:
 * return the given fallback pointer).
 */
const char *
ToString(std::span<char> buffer, SocketAddress address,
	 const char *fallback) noexcept;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer, without the port number.
 *
 * @return true on success
 */
bool
HostToString(std::span<char> buffer, SocketAddress address) noexcept;
