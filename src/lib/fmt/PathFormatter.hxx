// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "fs/Path.hxx"

#include <fmt/format.h>

#include <concepts>

template<std::convertible_to<Path> T>
struct fmt::formatter<T> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(Path path, FormatContext &ctx) {
		return formatter<string_view>::format(path.ToUTF8(), ctx);
	}
};
