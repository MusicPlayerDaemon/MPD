// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef PATH_FORMATTER_HXX
#define PATH_FORMATTER_HXX

#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"

#include <fmt/format.h>

template<>
struct fmt::formatter<Path> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(Path path, FormatContext &ctx) {
		return formatter<string_view>::format(path.ToUTF8(), ctx);
	}
};

template<>
struct fmt::formatter<AllocatedPath> : formatter<Path> {};

#endif
