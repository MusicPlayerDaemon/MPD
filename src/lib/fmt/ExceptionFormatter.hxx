// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef EXCEPTION_FORMATTER_HXX
#define EXCEPTION_FORMATTER_HXX

#include "util/Exception.hxx"

#include <fmt/format.h>

template<>
struct fmt::formatter<std::exception_ptr> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(std::exception_ptr e, FormatContext &ctx) {
		return formatter<string_view>::format(GetFullMessage(e), ctx);
	}
};

#endif
