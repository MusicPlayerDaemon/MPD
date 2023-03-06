// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_URI_HXX
#define MPD_DB_URI_HXX

#include <string_view>

static inline bool
isRootDirectory(std::string_view name) noexcept
{
	return name.empty() || (name.size() == 1 && name.front() == '/');
}

#endif
