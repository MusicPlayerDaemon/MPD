// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef THREAD_ID_FORMATTER_HXX
#define THREAD_ID_FORMATTER_HXX

#include <fmt/format.h>
#include <sstream>
#include <thread>

template<>
struct fmt::formatter<std::thread::id> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(std::thread::id id, FormatContext &ctx) {
		std::stringstream stm;
		stm << id;
		return formatter<string_view>::format(stm.str(), ctx);
	}
};

#endif // THREAD_ID_FORMATTER_HXX
