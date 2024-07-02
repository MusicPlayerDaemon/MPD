// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/format.h>

#include <exception>

template<>
struct fmt::formatter<std::exception_ptr> : formatter<string_view>
{
	auto format(std::exception_ptr e, format_context &ctx) const -> format_context::iterator;
};
