// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string>

class SocketAddress;

/**
 * Converts the specified socket address into a string in the form
 * "IP:PORT".
 */
[[gnu::pure]]
std::string
ToString(SocketAddress address) noexcept;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer, without the port number.
 */
[[gnu::pure]]
std::string
HostToString(SocketAddress address) noexcept;
