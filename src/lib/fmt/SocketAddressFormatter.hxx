// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/SocketAddress.hxx"

#include <fmt/format.h>

#include <concepts>

template<>
struct fmt::formatter<SocketAddress> : formatter<string_view>
{
	auto format(SocketAddress address, format_context &ctx) const -> format_context::iterator;
};

template<std::convertible_to<SocketAddress> T>
struct fmt::formatter<T> : formatter<SocketAddress>
{
};
